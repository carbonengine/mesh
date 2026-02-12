#include "skeleton.h"
#include "cmf/memallocator.h"
#include "options.h"
#include "transform.h"


/** @brief Imports a bone and its children into the CMF skeleton.
 * @param obj The FBX node representing the bone.
 * @param skeleton The CMF skeleton to populate.
 * @param parentIndex The index of the parent bone in the CMF skeleton.
 * @param allocator The memory allocator to use for allocations.
 * @param bones A vector to keep track of imported bones.
 * @param moveToOrigin Whether to move the bone to the origin (used optionally for the root bone).
 * @param system The coordinate system to use for transformations.
 */
void ImportBone( const ufbx_node& obj, cmf::Skeleton& skeleton, uint32_t parentIndex, cmf::MemoryAllocator& allocator, std::vector<const ufbx_node*>& bones, bool moveToOrigin, const CoordinateSystem& system )
{
	bones.push_back( &obj );

	cmf::Modify( skeleton.bones, allocator ).push_back( allocator.AllocateString( ToString( obj.name ) ) );
	cmf::Modify( skeleton.parents, allocator ).push_back( parentIndex );
	parentIndex = uint32_t( skeleton.bones.size() - 1 );

	Matrix globalTransform = ToMatrix( obj.geometry_to_world );
	auto invTransform = system.TransformMatrix( Inverse( globalTransform ) );
	cmf::Modify( skeleton.invBindTransforms, allocator ).push_back( invTransform );

	ufbx_transform transform = obj.local_transform;
	if( moveToOrigin )
	{
		transform.translation.x = 0.f;
		transform.translation.y = 0.f;
		transform.translation.z = 0.f;
		transform.rotation.x = 0.f;
		transform.rotation.y = 0.f;
		transform.rotation.z = 0.f;
		transform.rotation.w = 1.f;
	}
	Matrix localTransform = ToMatrix( transform );
	localTransform = system.TransformMatrix( localTransform );

	cmf::Transform restTransform;
	Decompose( restTransform.scale, restTransform.rotation, restTransform.position, localTransform );

	cmf::Modify( skeleton.restTransforms, allocator ).push_back( restTransform );

	for( auto& prop : obj.props.props )
	{
		if( ( prop.flags & UFBX_PROP_FLAG_USER_DEFINED ) == 0 || prop.type != UFBX_PROP_NUMBER )
		{
			continue;
		}
		auto name = ToString( prop.name );
		auto found = std::find_if( skeleton.boneMasks.begin(), skeleton.boneMasks.end(), [&]( const cmf::BoneMask& mask ) { return ToStdString( mask.name ) == name; } );
		if( found == skeleton.boneMasks.end() )
		{
			cmf::BoneMask mask;
			mask.name = allocator.AllocateString( name );
			cmf::Modify( skeleton.boneMasks, allocator ).push_back( mask );
			found = skeleton.boneMasks.end() - 1;
		}
		cmf::Modify( found->weights, allocator ).push_back( { parentIndex, float( prop.value_real ) } );
	}

	for( int i = 0; i < obj.children.count; ++i )
	{
		const auto child = obj.children.data[i];
		if( !child->bone )
		{
			continue;
		}
		if( std::find( begin( bones ), end( bones ), child ) != end( bones ) )
		{
			continue;
		}
		ImportBone( *child, skeleton, parentIndex, allocator, bones, false, system );
	}
}

/** @brief Imports a skeleton from an FBX node.
 * @param node The FBX node representing the root of the skeleton.
 * @param options The options for importing the skeleton.
 * @param allocator The memory allocator to use for allocations.
 * @param system The coordinate system to use for transformations.
 * @return A pair containing the imported CMF skeleton and a vector of FBX nodes representing the bones.
 */
std::pair<cmf::Skeleton, std::vector<const ufbx_node*>> ImportSkeleton( const ufbx_node& node, const SkeletonImportOptions& options, cmf::MemoryAllocator& allocator, const CoordinateSystem& system )
{
	cmf::Skeleton skeleton;
	skeleton.name = allocator.AllocateString( ToString( node.name ) );
	std::vector<const ufbx_node*> bones;
	ImportBone( node, skeleton, -1, allocator, bones, options.moveToOrigin, system );
	return { skeleton, bones };
}

std::pair<cmf::Span<cmf::Skeleton>, BoneMap> ImportSkeletons( const ufbx_scene& scene, const SkeletonImportOptions& options, cmf::MemoryAllocator& allocator, const CoordinateSystem& system )
{
	cmf::Span<cmf::Skeleton> skeletons;
	BoneMap boneMap;

	if( options.importSkeletons )
	{
		for( int i = 0; i < scene.nodes.count; ++i )
		{
			auto obj = scene.nodes[i];
			if( !obj->bone )
			{
				continue;
			}
			if( obj->parent && obj->parent->bone )
			{
				continue;
			}

			// Some skeletons are nested inside mesh nodes. We need to include the parent node in this case as if
            // it is a root bone to match legacy importer.
			if( obj->parent && ufbx_as_node( &obj->parent->element ) && obj->parent != scene.root_node )
			{
				obj = obj->parent;
			}

			if( !options.namedFilter( ToString( obj->name ) ) )
			{
				continue;
			}
			auto [skeleton, bones] = ImportSkeleton( *obj, options, allocator, system );
			cmf::Modify( skeletons, allocator ).push_back( skeleton );
			for( auto bone : bones )
			{
				boneMap[bone] = { uint32_t( skeletons.size() - 1 ), bone == obj };
			}
		}

		// To match the order of skeletons in the legacy importer
		std::reverse( skeletons.begin(), skeletons.end() );
	}
	return { skeletons, boneMap };
}

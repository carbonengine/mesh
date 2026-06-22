// Copyright © 2026 CCP ehf.

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
	const auto isRootBone = parentIndex == 0xffffffff;
	bones.push_back( &obj );

	cmf::Modify( skeleton.bones, allocator ).push_back( allocator.AllocateString( ToString( obj.name ) ) );
	cmf::Modify( skeleton.parents, allocator ).push_back( parentIndex );
	parentIndex = uint32_t( skeleton.bones.size() - 1 );

	Matrix localTransform = ToMatrix( obj.local_transform );
	if( isRootBone )
	{
		localTransform = ToMatrix( obj.node_to_world ) * system.m_transform;
	}
	cmf::Transform restTransform;
	Decompose( restTransform.scale, restTransform.rotation, restTransform.position, localTransform );
	restTransform.position *= system.m_scale;
	if( moveToOrigin )
	{
		restTransform.position = Vector3( 0, 0, 0 );
		restTransform.rotation = Quaternion( 0, 0, 0, 1 );
	}

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
		cmf::Modify( found->weights, allocator ).push_back( { parentIndex, std::clamp( float( prop.value_real ), 0.0f, 1.0f ) } );
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

	skeleton.invBindTransforms = allocator.AllocateSpan<Matrix>( skeleton.bones.size() );
	for( uint32_t i = 0; i < skeleton.bones.size(); ++i )
	{
		auto localTransform = TransformationMatrix( skeleton.restTransforms[i].scale, skeleton.restTransforms[i].rotation, skeleton.restTransforms[i].position );
		Matrix parentInvTransform = IdentityMatrix();
		if( skeleton.parents[i] != 0xffffffff )
		{
			parentInvTransform = skeleton.invBindTransforms[skeleton.parents[i]];
		}
		auto invTransform = parentInvTransform * Inverse( localTransform );
		skeleton.invBindTransforms[i] = invTransform;
	}

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
	}
	return { skeletons, boneMap };
}

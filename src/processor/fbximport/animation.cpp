// Copyright © 2026 CCP ehf.

#include "animation.h"
#include "cmf/declutils.h"
#include "cmf/animation.h"


/**
 * @brief Converts a list of baked animation keys into an AnimationCurve.
 *
 * This function takes a list of baked animation keys (such as ufbx_baked_###_list) and converts them into a cmf::AnimationCurve.
 * The provided @p transformValue function is used to convert each key's value from the baked format into the desired output format,
 * which may include applying coordinate system transformations or other conversions.
 *
 * @tparam K The type of the key list, one of ufbx_baked_###_list.
 * @tparam T The type of the transformation function or functor, which must be callable with a key value.
 * @param keys The list of baked animation keys to convert.
 * @param allocator The memory allocator used for allocating curve data.
 * @param transformValue A callable that transforms a key's value to the output format.
 * @return cmf::AnimationCurve The resulting animation curve with transformed values and corresponding knots.
 *
 * @note The value dimension of the resulting curve is determined by the size of the transformed value type.
 */
template <typename K, typename T>
cmf::AnimationCurve ImportCurve( const K& keys, cmf::MemoryAllocator& allocator, T&& transformValue )
{
	using VT = decltype( transformValue( keys[0].value ) );
	cmf::AnimationCurve curve;
	curve.valueDimension = sizeof( VT ) / sizeof( float ); // NOLINT(bugprone-sizeof-expression)
	curve.interpolation = cmf::Interpolation::Linear;
	for( int k = 0; k < keys.count; ++k )
	{
		cmf::Modify( reinterpret_cast<cmf::Span<float>&>( curve.knots ), allocator ).emplace_back( float( keys[k].time ) );
		cmf::Modify( reinterpret_cast<cmf::Span<VT>&>( curve.values ), allocator ).emplace_back( transformValue( keys[k].value ) );
	}
	curve.knotCount = uint32_t( curve.knots.size() / sizeof( float ) );
	return curve;
}

/**
 * @brief Checks if a given element is a morph animation target: a root node of a skeleton.
 * 
 * By convention, morph target animations are targeting properties of a skeleton root node.
 * The function checks if the element with the given ID is a skeleton root node.
 * 
 * @param scene The FBX scene containing the elements.
 * @param elementId The ID of the element to check.
 * @param boneMap A map of found bones in the scene.
 * @return true if the element is a morph animation target, false otherwise.
 */
bool IsMorphAnimationTarget( const ufbx_scene& scene, uint32_t elementId, const BoneMap& boneMap )
{
	auto el = scene.elements[elementId];
	if( el->type != UFBX_ELEMENT_NODE )
	{
		return false;
	}
	auto found = boneMap.find( scene.nodes[el->typed_id] );
	if( found == boneMap.end() )
	{
		return false;
	}
	return found->second.isRoot;
}

/** 
* @brief Imports skeletal animations from a baked animation into animation channels and curves.
* 
* This function takes a baked animation and converts it into a set of animation channels and curves
* that can be used to drive a skeleton. 
*
* @param channels The span of animation channels to populate.
* @param curves The span of animation curves to populate.
* @param bakedAnim The baked animation data to import.
* @param moveToOrigin Whether to record root bone animation relative to the first frame of the animation.
* @param scene The FBX scene containing the animation data.
* @param boneMap A map of bones in the scene.
* @param allocator The memory allocator used for allocating curve data.
* @param system The coordinate system to use for transforming animation data.
*/
void ImportSkeletalAnimations(
	cmf::Span<cmf::AnimationChannel>& channels,
	cmf::Span<cmf::AnimationCurve>& curves,
	const ufbx_baked_anim& bakedAnim,
	bool moveToOrigin,
	const ufbx_scene& scene,
	const BoneMap& boneMap,
	cmf::MemoryAllocator& allocator,
	const CoordinateSystem& system )
{
	for( size_t i = 0; i < bakedAnim.nodes.count; i++ )
	{
		ufbx_baked_node* bake_node = &bakedAnim.nodes.data[i];
		ufbx_node* target = scene.nodes.data[bake_node->typed_id];
		if( boneMap.find( target ) == boneMap.end() )
		{
			continue;
		}

		auto boneName = ToString( target->name );
		cmf::AnimationChannel position;
		position.target = allocator.AllocateString( boneName );
		position.targetType = cmf::AnimationChannelTargetType::BonePosition;
		position.curveIndex = uint32_t( curves.size() );
		cmf::Modify( channels, allocator ).push_back( position );

		cmf::AnimationChannel rotation;
		rotation.target = allocator.AllocateString( boneName );
		rotation.targetType = cmf::AnimationChannelTargetType::BoneRotation;
		rotation.curveIndex = uint32_t( curves.size() ) + 1;
		cmf::Modify( channels, allocator ).push_back( rotation );

		cmf::AnimationChannel scale;
		scale.target = allocator.AllocateString( boneName );
		scale.targetType = cmf::AnimationChannelTargetType::BoneScale;
		scale.curveIndex = uint32_t( curves.size() ) + 2;
		cmf::Modify( channels, allocator ).push_back( scale );

		const auto isRootBone = boneMap.find( target )->second.isRoot;
		auto moveBoneToOrigin = moveToOrigin && boneMap.find( target )->second.isRoot;

		Vector3 initialPosition = { 0, 0, 0 };
		Quaternion initialRotation = { 0, 0, 0, 1 };
		if( moveBoneToOrigin )
		{
			initialPosition = system.TransformPoint( ToVector3( bake_node->translation_keys[0].value ) );
			initialRotation = system.TransformRotation( ToQuaternion( bake_node->rotation_keys[0].value ) );
		}
		auto parentTransform = ( target->parent ? ToMatrix( target->parent->node_to_world ) : IdentityMatrix() ) * system.m_transform;

		const cmf::AnimationCurve outPosition = ImportCurve( bake_node->translation_keys, allocator, [&]( const ufbx_vec3& v ) {
			if( isRootBone )
			{
				if( moveBoneToOrigin )
				{
					return system.TransformPoint( ToVector3( v ) ) - initialPosition;
				}
				return TransformCoord( ToVector3( v ), parentTransform ) * system.m_scale;
			}
			return ToVector3( v ) * system.m_scale;
		} );
		const cmf::AnimationCurve outRotation = ImportCurve( bake_node->rotation_keys, allocator, [&]( const ufbx_quat& q ) {
			if( isRootBone )
			{
				if( moveBoneToOrigin )
				{
					return system.TransformRotation( ToQuaternion( q ) ) * Inverse( initialRotation );
				}
				auto m = RotationMatrix( ToQuaternion( q ) ) * parentTransform;
				Vector3 position, scale;
				Quaternion rotation;
				Decompose( scale, rotation, position, m );
				return rotation;
			}
			return ToQuaternion( q );
		} );
		const cmf::AnimationCurve outScale = ImportCurve( bake_node->scale_keys, allocator, [&]( const ufbx_vec3& v ) {
			return ToVector3( v );
		} );

		cmf::Modify( curves, allocator ).push_back( outPosition );
		cmf::Modify( curves, allocator ).push_back( outRotation );
		cmf::Modify( curves, allocator ).push_back( outScale );
	}
}

/**
* @brief Imports morph target animations from a baked animation into animation channels and curves.
*
* This function extracts morph target weight animations from the baked animation and converts them into a set of animation channels and curves
* that can be used to drive morph targets. 
*
* @param channels The span of animation channels to populate.
* @param curves The span of animation curves to populate.
* @param bakedAnim The baked animation data to import.
* @param scene The FBX scene containing the animation data.
* @param boneMap A map of bones in the scene.
* @param allocator The memory allocator used for allocating curve data.
*/
void ImportMorphAnimations( cmf::Span<cmf::AnimationChannel>& channels, cmf::Span<cmf::AnimationCurve>& curves, const ufbx_baked_anim& bakedAnim, const ufbx_scene& scene, const BoneMap& boneMap, cmf::MemoryAllocator& allocator )
{
	for( auto& element : bakedAnim.elements )
	{
		if( !IsMorphAnimationTarget( scene, element.element_id, boneMap ) )
		{
			continue;
		}
		auto el = scene.elements[element.element_id];
		for( auto& bakedProp : element.props )
		{
			if( bakedProp.constant_value )
			{
				// Property is not animated, skip it
				continue;
			}
			auto name = ToString( bakedProp.name );
			auto prop = std::find_if( el->props.props.begin(), el->props.props.end(), [&]( const ufbx_prop& p ) { return ToString( p.name ) == name; } );
			if( prop == el->props.props.end() || ( prop->flags & UFBX_PROP_FLAG_USER_DEFINED ) == 0 || prop->type != UFBX_PROP_NUMBER )
			{
				continue;
			}

			cmf::AnimationCurve curve = ImportCurve( bakedProp.keys, allocator, []( const ufbx_vec3& v ) {
				return float( v.x );
			} );
			cmf::Modify( curves, allocator ).push_back( curve );

			cmf::AnimationChannel weight;
			weight.target = allocator.AllocateString( name );
			weight.targetType = cmf::AnimationChannelTargetType::MorphTarget;
			weight.curveIndex = uint32_t( curves.size() - 1 );
			cmf::Modify( channels, allocator ).push_back( weight );
		}
	}
}

cmf::Span<cmf::Animation> ImportAnimations( const ufbx_scene& scene, const BoneMap& boneMap, const AnimationImportOptions& options, cmf::MemoryAllocator& allocator, const CoordinateSystem& system )
{
	cmf::Span<cmf::Animation> outAnims;

	for( auto animStack : scene.anim_stacks )
	{
		if( !animStack->anim )
		{
			continue;
		}

		cmf::Animation outAnim;

		auto name = ToString( animStack->name );
		if( !options.namedFilter( name ) )
		{
			continue;
		}
		outAnim.name = allocator.AllocateString( name );
		outAnim.duration = float( animStack->anim->time_end - animStack->anim->time_begin );

		ufbx_bake_opts opts = { 0 };
		opts.trim_start_time = true;
		opts.step_handling = UFBX_BAKE_STEP_HANDLING_IDENTICAL_TIME;
		opts.key_reduction_enabled = options.reduceKeyframes;
		opts.key_reduction_rotation = true;
		opts.key_reduction_threshold = options.keyReductionTolerance;
		auto* bakedAnim = ufbx_bake_anim( &scene, animStack->anim, &opts, nullptr );
		ImportSkeletalAnimations(
			outAnim.channels,
			outAnim.curves,
			*bakedAnim,
			options.moveToOrigin,
			scene,
			boneMap,
			allocator,
			system );
		ImportMorphAnimations( outAnim.channels, outAnim.curves, *bakedAnim, scene, boneMap, allocator );

		if( !outAnim.channels.empty() )
		{
			cmf::Modify( outAnims, allocator ).push_back( outAnim );
		}
		ufbx_free_baked_anim( bakedAnim );
	}

	// To match the order of animations in the legacy importer
	std::reverse( outAnims.begin(), outAnims.end() );

	// Remove duplicate curves
	for( auto& anim : outAnims )
	{
		cmf::RemoveDuplicateCurves( anim, allocator );
	}
	if( options.optimizeFormat )
	{
		for( auto& anim : outAnims )
		{
			for( auto& curve : anim.curves )
			{
				cmf::OptimizeCurveFormat( curve, options.keyTolerance, options.valueTolerance, allocator );
			}
		}
	}
	return outAnims;
}

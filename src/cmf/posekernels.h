// Copyright © 2026 CCP ehf.

#pragma once

#include "cmf/cmf.h"
#include "cmf/transforms.h"

#include <cstddef>

// Internal pose blending kernels that operate on raw bone transform arrays. They are shared by the public
// SkeletonPose-based blending functions and by the animation network evaluation. Output arrays may alias
// any of the input arrays: every bone is read before it is written.

namespace cmf::detail
{

inline void BlendTransforms( Transform* out, const Transform* poseA, const Transform* poseB, size_t boneCount, float alpha )
{
	for( size_t i = 0; i < boneCount; ++i )
	{
		out[i] = Lerp( poseA[i], poseB[i], alpha );
	}
}

inline void BlendTransformsWeighted( Transform* out, const Transform* poseA, const Transform* poseB, const float* boneWeights, size_t boneCount, float alpha )
{
	for( size_t i = 0; i < boneCount; ++i )
	{
		out[i] = Lerp( poseA[i], poseB[i], boneWeights[i] * alpha );
	}
}

inline Transform BlendAdditiveTransform( const Transform& pose, const Transform& base, const Transform& additive, float alpha )
{
	const auto difference = Multiply( Inverse( base ), additive );
	return Multiply( pose, Lerp( Transform{}, difference, alpha ) );
}

inline void BlendAdditiveTransforms( Transform* out, const Transform* pose, const Transform* base, const Transform* additive, size_t boneCount, float alpha )
{
	for( size_t i = 0; i < boneCount; ++i )
	{
		out[i] = BlendAdditiveTransform( pose[i], base[i], additive[i], alpha );
	}
}

inline void BlendAdditiveTransformsWeighted( Transform* out, const Transform* pose, const Transform* base, const Transform* additive, const float* boneWeights, size_t boneCount, float alpha )
{
	for( size_t i = 0; i < boneCount; ++i )
	{
		out[i] = BlendAdditiveTransform( pose[i], base[i], additive[i], boneWeights[i] * alpha );
	}
}

}

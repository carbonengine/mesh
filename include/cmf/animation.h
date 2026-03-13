#pragma once

#include "cmf.h"
#include <vector>

namespace cmf
{

struct SkeletonPose
{
	const Skeleton* skeleton = nullptr;
	std::vector<Transform> boneTransforms;
};


/** @brief Samples a scalar curve at the given time.
* The curve must be a well-formed object, and have a value dimension of 1. The function does not perform any validation on the curve, 
* and may return invalid results if the curve is malformed or has an incompatible value dimension.
* 
* @param curve The animation curve to sample.
* @param time The time at which to sample the curve.
* @return The sampled value of the curve at the specified time.
*/
CARBON_MESH_EXPORT float SampleScalarCurve( const cmf::AnimationCurve& curve, float time );

/** @brief Samples a 2 component vector curve at the given time.
* The curve must be a well-formed object, and have a value dimension of 2. The function does not perform any validation on the curve, 
* and may return invalid results if the curve is malformed or has an incompatible value dimension.
* 
* @param curve The animation curve to sample.
* @param time The time at which to sample the curve.
* @return The sampled value of the curve at the specified time.
*/
CARBON_MESH_EXPORT Vector2 SampleVector2Curve( const cmf::AnimationCurve& curve, float time );

/** @brief Samples a 3 component vector curve at the given time.
* The curve must be a well-formed object, and have a value dimension of 3. The function does not perform any validation on the curve, 
* and may return invalid results if the curve is malformed or has an incompatible value dimension.
* 
* @param curve The animation curve to sample.
* @param time The time at which to sample the curve.
* @return The sampled value of the curve at the specified time.
*/
CARBON_MESH_EXPORT Vector3 SampleVector3Curve( const cmf::AnimationCurve& curve, float time );


/** @brief Samples a 4 component vector curve at the given time.
* The curve must be a well-formed object, and have a value dimension of 4. The function does not perform any validation on the curve, 
* and may return invalid results if the curve is malformed or has an incompatible value dimension.
* 
* @param curve The animation curve to sample.
* @param time The time at which to sample the curve.
* @return The sampled value of the curve at the specified time.
*/
CARBON_MESH_EXPORT Vector4 SampleVector4Curve( const cmf::AnimationCurve& curve, float time );


/** @brief Samples a quaternion curve at the given time.
* For linear curves the function performs SLerp interpolation between quaternion values. The curve 
* must be a well-formed object, and have a value dimension of 4. The function does not perform any validation on the curve, 
* and may return invalid results if the curve is malformed or has an incompatible value dimension.
* 
* @param curve The animation curve to sample.
* @param time The time at which to sample the curve.
* @return The sampled value of the curve at the specified time.
*/
CARBON_MESH_EXPORT Quaternion SampleQuaternionCurve( const cmf::AnimationCurve& curve, float time );

/** @brief Samples a skeletal animation to produce a new skeleton pose at the given time.
* The function iterates through the animation channels, identifies those that target bone transformations, and samples the 
* corresponding curves to update the bone transforms in the output pose. It will not change the transforms of bones that are 
* not targeted by any animation channel. The function assumes that the input pose is already initialized with a valid skeleton 
* and bone transforms (e.g., from the rest pose), and it modifies the bone transforms in place based on the sampled animation data.
* The function does not perform any validation on the parameters.
* 
* @param pose The skeleton pose to update.
* @param animation The animation to sample.
* @param curves The animation curves referenced by the animation.
* @param time The time at which to sample the animation.
*/
CARBON_MESH_EXPORT void SampleAnimation( SkeletonPose& pose, const cmf::Animation& animation, const Span<AnimationCurve>& curves, float time );

/** @brief Resets a skeleton pose to its rest pose.
* The function iterates through the bones of the skeleton and sets the corresponding transforms in the pose to the rest pose values.
* 
* @param outPose The skeleton pose to reset.
* @param skeleton The skeleton whose rest pose to use.
*/
CARBON_MESH_EXPORT void RestPose( SkeletonPose& outPose, const Skeleton& skeleton );

/** @brief Blends two skeleton poses together using a specified alpha value.
* The function interpolates between the transforms of the two input poses based on the alpha value, producing a new pose.
* Both poses must reference the same skeleton. The function does not perform any validation on the parameters.
* 
* @param outPose The output skeleton pose.
* @param poseA The first input skeleton pose.
* @param poseB The second input skeleton pose.
* @param alpha The interpolation factor between the two poses.
*/
CARBON_MESH_EXPORT void BlendPoses( SkeletonPose& outPose, const SkeletonPose& poseA, const SkeletonPose& poseB, float alpha );

/** @brief Blends two skeleton poses together using per-bone weights.
* The function interpolates between the transforms of the two input poses based on the provided bone weights, producing a new pose.
* Both poses must reference the same skeleton, and the bone weights must correspond to the bones in the skeleton. The function does not perform any validation on the parameters.
* 
* @param outPose The output skeleton pose.
* @param poseA The first input skeleton pose.
* @param poseB The second input skeleton pose.
* @param boneWeights The per-bone weights for blending.
*/
CARBON_MESH_EXPORT void BlendPoses( SkeletonPose& outPose, const SkeletonPose& poseA, const SkeletonPose& poseB, const Span<BoneWeight>& boneWeights );

/** @brief Computes the world transformation matrices for each bone in a skeleton pose.
* The function iterates through the bones of the skeleton pose and computes the world transformation matrices based on the local transforms and the hierarchy of the skeleton.
* 
* @param outWorldTransforms The output vector of world transformation matrices.
* @param pose The skeleton pose to compute the world transforms for.
*/
CARBON_MESH_EXPORT void ComputeWorldTransforms( std::vector<Matrix>& outWorldTransforms, const SkeletonPose& pose );

}
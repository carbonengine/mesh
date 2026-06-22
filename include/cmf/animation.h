// Copyright © 2026 CCP ehf.

#pragma once

#include "cmf.h"
#include "bufferstreams.h"
#include <vector>
#include <functional>
#include <cstdint>

namespace cmf
{

struct SkeletonPose
{
	/** Reference skeleton */
	const Skeleton* skeleton = nullptr;
	/** Flattened vector of bone local transforms: one transform per skeleton bone */
	std::vector<Transform> boneTransforms;
};

struct BoneWeights
{
	/** Reference skeleton */
	const Skeleton* skeleton = nullptr;
	/** Flattened vector of bone weights: one weight per skeleton bone */
	std::vector<float> boneWeights;
};

/**
 * @brief Curve evaluation cache for a single curve. Stores the last evaluated knot interval and corresponding values to optimize subsequent evaluations that fall within the same interval.
 * @tparam T The type of the values stored in the cache.
 */
template <typename T>
struct CurveEvaluationCache
{
	static constexpr uint32_t INVALID_INDEX = 0xffffffff;

	uint32_t knotIndex = INVALID_INDEX;
	float knot0 = 0.f;
	float knot1 = 0.f;
	T value0 = {};
	T value1 = {};
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

/**
 * @brief Removes duplicate curves from an animation. 
 * Considers curves identical if they are bitwise identical. The function modifies animation parameter in-place.
 * 
 * @param animation The animation from which to remove duplicate curves.
 * @param allocator The memory allocator to use for the operation.
 */
CARBON_MESH_EXPORT void RemoveDuplicateCurves( cmf::Animation& animation, cmf::MemoryAllocator& allocator );

/**
 * @brief Optimizes the knot and/or value format of an animation curve to reduce memory usage. Tries to choose the smallest possible numeric 
 * format for the curve's knots and values (8 or 16 bit fixed point formats or 16-bit float format) based on the range and precision of the data, 
 * while ensuring that the optimized curve produces results within the specified tolerances. The function modifies the input curve in-place.
 * 
 * @param curve The animation curve to optimize. This parameter is modified in place.
 * @param keyTolerance The tolerance value used for keyframe time optimization.
 * @param tolerance The general tolerance value used during optimization.
 * @param allocator The memory allocator used for internal operations.
 * @return This function does not return a value.
 */
CARBON_MESH_EXPORT void OptimizeCurveFormat( cmf::AnimationCurve& curve, float keyTolerance, float tolerance, cmf::MemoryAllocator& allocator );

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

/** @brief Extracts per-bone weights from a bone mask for a given skeleton.
* The function finds the bone mask by name in the provided skeleton and creates a flattened list of bone weights based on the 
* mask (bones not referenced in the mask get zero weights). If the mask is not found, the function returns a BoneWeights structure with all weights set to zero.
* @param skeleton The skeleton containing the bone mask.
* @param boneMask The name of the bone mask to extract weights from.
*/
CARBON_MESH_EXPORT BoneWeights ExtractBoneWeights( const Skeleton& skeleton, const std::string_view& boneMask );

/** @brief Blends two skeleton poses together using per-bone weights.
* The function interpolates between the transforms of the two input poses based on the provided bone weights, producing a new pose.
* Both poses must reference the same skeleton, and the bone weights must correspond to the bones in the skeleton. The function does not perform any validation on the parameters.
* 
* @param outPose The output skeleton pose.
* @param poseA The first input skeleton pose.
* @param poseB The second input skeleton pose.
* @param boneWeights The per-bone weights for blending.
* @param alpha The interpolation factor between the two poses (multiplied by the per-bone weights).
*/
CARBON_MESH_EXPORT void BlendPoses( SkeletonPose& outPose, const SkeletonPose& poseA, const SkeletonPose& poseB, const BoneWeights& boneWeights, float alpha );


/** @brief Blends an additive pose onto a base skeleton pose using a uniform alpha value.
* The function computes the difference between the additive pose and the base pose (by multiplying with the inverse of the base transform),
* then interpolates between the input pose and the computed additive difference using the specified alpha value.
* All poses must reference the same skeleton. The function does not perform any validation on the parameters.
*
* @param outPose The output skeleton pose.
* @param poseA The input skeleton pose to blend onto.
* @param basePose The base pose of the additive animation (used to compute the additive difference).
* @param additivePose The additive pose to blend in.
* @param alpha The interpolation factor controlling the strength of the additive blend.
*/
CARBON_MESH_EXPORT void BlendAdditivePose( SkeletonPose& outPose, const SkeletonPose& poseA, const SkeletonPose& basePose, const SkeletonPose& additivePose, float alpha );


/** @brief Blends an additive pose onto a base skeleton pose using per-bone weights.
* The function computes the difference between the additive pose and the base pose (by multiplying with the inverse of the base transform),
* then interpolates between the input pose and the computed additive difference using the per-bone weights.
* All poses must reference the same skeleton, and the bone weights must correspond to the bones in the skeleton.
* The function does not perform any validation on the parameters.
*
* @param outPose The output skeleton pose.
* @param poseA The input skeleton pose to blend onto.
* @param basePose The base pose of the additive animation (used to compute the additive difference).
* @param additivePose The additive pose to blend in.
* @param boneWeights The per-bone weights controlling the strength of the additive blend for each bone.
* @param alpha The interpolation factor controlling the strength of the additive blend (multiplied by the per-bone weights).
*/
CARBON_MESH_EXPORT void BlendAdditivePose( SkeletonPose& outPose, const SkeletonPose& poseA, const SkeletonPose& basePose, const SkeletonPose& additivePose, const BoneWeights& boneWeights, float alpha );

/** @brief Computes the world transformation matrices for each bone in a skeleton pose.
* The function iterates through the bones of the skeleton pose and computes the world transformation matrices based on the local transforms and the hierarchy of the skeleton.
* 
* @param outWorldTransforms The output vector of world transformation matrices.
* @param pose The skeleton pose to compute the world transforms for.
*/
CARBON_MESH_EXPORT void ComputeWorldTransforms( std::vector<Matrix>& outWorldTransforms, const SkeletonPose& pose );


/** @brief Plays and manages a single animation for a skeleton.
* The AnimationPlayer class provides functionality to play and sample animations for a given skeleton.
* The lifetime of the skeleton and animation data passed to the constructor must exceed the lifetime of the AnimationPlayer instance. 
* The class does not take ownership of the skeleton or animation data, and it is the caller's responsibility to ensure that they remain 
* valid while the AnimationPlayer is in use.
* The class supports looping and playback speed adjustment.
*/
class AnimationPlayer
{
public:
	constexpr static uint32_t INFINITE_LOOP = 0;

	/** @brief Constructs an AnimationPlayer for a given skeleton and animation.
    * @param skeleton The skeleton to play the animation on.
    * @param animation The animation to play.
    */
	CARBON_MESH_EXPORT AnimationPlayer( const cmf::Skeleton& skeleton, const cmf::Animation& animation );
	CARBON_MESH_EXPORT ~AnimationPlayer();

	AnimationPlayer( const AnimationPlayer& ) = delete;
	AnimationPlayer( AnimationPlayer&& ) = delete;
	AnimationPlayer& operator=( const AnimationPlayer& ) = delete;
	AnimationPlayer& operator=( AnimationPlayer&& ) = delete;

	[[nodiscard]] CARBON_MESH_EXPORT uint32_t GetLoopCount() const;
	CARBON_MESH_EXPORT void SetLoopCount( uint32_t loopCount );
	[[nodiscard]] CARBON_MESH_EXPORT float GetSpeed() const;
	CARBON_MESH_EXPORT void SetSpeed( float speed );
	[[nodiscard]] CARBON_MESH_EXPORT float GetStartTime() const;
	CARBON_MESH_EXPORT void SetStartTime( float startTime );
	[[nodiscard]] CARBON_MESH_EXPORT float GetStopTime() const;
	CARBON_MESH_EXPORT void SetStopTime( float stopTime );
	[[nodiscard]] CARBON_MESH_EXPORT bool GetExtrapolateBefore() const;
	CARBON_MESH_EXPORT void SetExtrapolateBefore( bool extrapolateBefore );
	[[nodiscard]] CARBON_MESH_EXPORT bool GetExtrapolateAfter() const;
	CARBON_MESH_EXPORT void SetExtrapolateAfter( bool extrapolateAfter );

	[[nodiscard]] CARBON_MESH_EXPORT float GetLoopDuration() const;

	[[nodiscard]] CARBON_MESH_EXPORT const cmf::Skeleton& GetSkeleton() const;

	/** The function calculates the remaining time for the animation to finish based on the current time and the animation parameters. */
	[[nodiscard]] CARBON_MESH_EXPORT float GetDurationLeft( float time ) const;

	/** The function checks if the animation is active at the given time based on the start time, stop time, and extrapolation settings. */
	[[nodiscard]] CARBON_MESH_EXPORT bool IsActive( float time ) const;

	/** The function returns the current loop index based on the current time and the animation parameters. */
	[[nodiscard]] CARBON_MESH_EXPORT int32_t GetLoopIndex( float time ) const;
	/** The function returns the local time within the current loop based on the current time and the animation parameters. */
	[[nodiscard]] CARBON_MESH_EXPORT float GetLocalTime( float time ) const;

	/** @brief The function samples the animation at the given time and outputs the resulting pose. 
    * The output pose should be initialized with the same skeleton as the one used to construct the AnimationPlayer, and it should have valid bone transforms (e.g., from the rest pose) before calling this function. 
    * The function will modify the bone transforms in place based on the sampled animation data, but only for bones referenced in the animation. The function does not perform any validation on the parameters.
    * @param outPose The output pose to store the sampled animation data.
    * @param time The time at which to sample the animation.
    */
	CARBON_MESH_EXPORT bool Sample( SkeletonPose& outPose, float time ) const;

	/** @brief The function samples the animation at the given local time (time within the current loop) and outputs the resulting pose.
    * The output pose should be initialized with the same skeleton as the one used to construct the AnimationPlayer, and it should have valid bone transforms (e.g., from the rest pose) before calling this function. 
    * The function will modify the bone transforms in place based on the sampled animation data, but only for bones referenced in the animation. The function does not perform any validation on the parameters.
    * @param outPose The output pose to store the sampled animation data.
    * @param localTime The local time within the current loop at which to sample the animation.
    */
	CARBON_MESH_EXPORT void SampleAtLocalTime( SkeletonPose& outPose, float localTime ) const;

	/** @brief Rebases internal clocks by a given delta time.
    * @param deltaTime The amount of time to rebase the clocks by.
    */
	CARBON_MESH_EXPORT void RebaseClocks( float deltaTime );

private:
	template <typename T>
	struct CurveAccessor
	{
		cmf::ConstBufferElementStream<float> knots;
		cmf::ConstBufferElementStream<T> values;
		Interpolation interpolation = Interpolation::Linear;
		uint32_t targetIndex = 0;
		mutable CurveEvaluationCache<T> cache;
	};

	void AdjustStopTime();

	std::vector<CurveAccessor<Vector3>> m_positionCurves;
	std::vector<CurveAccessor<Vector4>> m_rotationCurves;
	std::vector<CurveAccessor<Vector3>> m_scaleCurves;

	float m_startTime = 0.f;
	float m_stopTime = 0.f;
	uint32_t m_loopCount = INFINITE_LOOP;
	float m_speed = 1.f;
	float m_loopDuration = 0.f;
	bool m_explicitStopTime = false;
	bool m_extrapolateBefore = false;
	bool m_extrapolateAfter = false;
	size_t m_playerIndex = 0;

	const cmf::Skeleton* m_skeleton = nullptr;
};

/** @brief Manages multiple animations for a skeleton, allowing them to be played one after another.
* The class does not support blending between animations, and it is the caller's responsibility to ensure that the 
* animations are sampled in the correct order to achieve the desired results. The class does not take ownership of 
* the skeleton or animation data, and it is the caller's responsibility to ensure that they remain valid while the AnimationSequencer is in use.
*/
class AnimationSequencer
{
public:
	/** @brief Constructs an AnimationSequencer for a given skeleton.
    * @param skeleton The skeleton to play animations on.
    */
	CARBON_MESH_EXPORT AnimationSequencer( const cmf::Skeleton& skeleton );

	/** @brief Plays an animation on the sequencer.
    * @param animation The animation to play.
    * @return A shared pointer to the AnimationPlayer managing the animation.
    */
	CARBON_MESH_EXPORT std::shared_ptr<AnimationPlayer> PlayAnimation( const cmf::Animation& animation );

	/** @brief Stops an animation and removes it from the sequencer.
    * @param player The animation to remove, previosly created by PlayAnimation.
    */
	CARBON_MESH_EXPORT void StopAnimation( const std::shared_ptr<AnimationPlayer>& player );

	/** @brief Enumerates all currently active animations in the sequencer and invokes a callback for each one.
    * @param callback The callback to invoke for each active animation.
    */
	CARBON_MESH_EXPORT void EnumerateAnimations( const std::function<void( const std::shared_ptr<AnimationPlayer>& )>& callback );

	/** @brief Samples animations in the sequencer at the given time and outputs the resulting pose.
    * Only samples the first active animation. The output pose should be initialized with the same skeleton as the one used to construct the AnimationSequencer, 
    * and it should have valid bone transforms (e.g., from the rest pose) before calling this function. The function will modify the bone transforms in place 
    * based on the sampled animation data, but only for bones referenced in the animation. The function does not perform any validation on the parameters.
    * @param outPose The output pose to store the sampled animation data.
    * @param time The time at which to sample the animations.
    */
	CARBON_MESH_EXPORT void Sample( SkeletonPose& outPose, float time );

	/** @brief Removes animations that have finished playing.
    * @param time The current time used to determine which animations have finished.
    */
	CARBON_MESH_EXPORT void RemoveFinishedAnimations( float time );

	[[nodiscard]] CARBON_MESH_EXPORT const cmf::Skeleton& GetSkeleton() const;

private:
	const cmf::Skeleton* m_skeleton = nullptr;
	std::vector<std::shared_ptr<AnimationPlayer>> m_animations;
};

/** @brief Rebases the internal clocks of all active AnimationPlayer instances by a given delta time.
 * @param deltaTime The amount of time to rebase the clocks by.
 */
CARBON_MESH_EXPORT void RebaseAllAnimationPlayerClocks( float deltaTime );

}

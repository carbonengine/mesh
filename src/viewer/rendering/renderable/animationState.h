// Copyright © 2026 CCP ehf.

#pragma once

#include "../renderer.h"
#include "../camera.h"
#include "../vulkan/commandbuffer.h"
#include "primitive.h"
#include <cmf/animation.h>

/// <summary>
/// This class handles the animation state for a single skeleton,
/// including the current pose, the active animation player, and the renderables for visualizing the bones and joints.
/// It provides functions to set the active animation, update the pose based on the animation time,
/// and respond to changes in selected bones.
/// Each instance of this class corresponds to one skeleton in the model,
/// and it is responsible for managing the animation state and visualization for that skeleton.
/// </summary>
class AnimationState
{
public:
	AnimationState( uint8_t skeletonIndex, const cmf::Skeleton& skeleton, std::shared_ptr<const Renderer> renderer );
	void Initialize( AppState& appState );

	void SetAnimation( const cmf::Animation* animation );
	void Update( float animationTime );
	void RenderNoDepthDebug( GraphicsCommandBuffer& commandBuffer, const AppState& appState, const Camera& camera );

	void SetAnimationOwner( std::shared_ptr<CmfContent> owner );

	uint8_t GetSkeletonIndex() const;
	[[nodiscard]] const cmf::Animation* GetAnimation() const;
	[[nodiscard]] const std::array<Matrix, 0xFF>& GetBoneTransforms() const;
	[[nodiscard]] const cmf::Span<cmf::String>& GetBoneNames() const;

private:
	void UpdateAnimation( float animationTime, AppState& appState );
	const cmf::Animation* GetActiveAnimation( AppState& appState );
	void CreateBoneMapping();
	void ClearBoneMapping();

	void SetSelectedBones( const std::vector<uint32_t>& selectedBones );
	uint8_t m_skeletonIndex{ 0 };

	cmf::SkeletonPose m_pose{};
	std::unique_ptr<cmf::AnimationPlayer> m_animationPlayer{ nullptr };
	const cmf::Skeleton& m_skeleton;
	std::unique_ptr<cmf::Skeleton> m_animationOverrideSkeleton{ nullptr };
	const cmf::Animation* m_animation{ nullptr };

	std::vector<Matrix> m_boneWorldTransforms{};
	std::array<Matrix, 0xFF> m_boneTransforms;
	// mapping from the model skeleton to the animation skeleton
	std::vector<std::int32_t> m_modelBoneToAnimationBoneMapping{};

	std::unique_ptr<PrimitiveRenderable> m_boneRenderable;
	std::unique_ptr<PrimitiveRenderable> m_jointRenderable;
	std::unique_ptr<PrimitiveRenderable> m_axisRenderable;
};
// Copyright © 2026 CCP ehf.

#pragma once

#include "../renderer.h"
#include "../vulkan/commandbuffer.h"
#include "mesh.h"
#include "animationState.h"
#include "debugDrawSettings.h"

#include <functional>


class ModelRenderable
{
public:
	/// Called for every morph weight driven by an animation curve: (mesh index, morph index, weight)
	using MorphWeightCallback = std::function<void( size_t, uint32_t, float )>;

	ModelRenderable( const std::shared_ptr<CmfContent>& data, const std::shared_ptr<const Renderer>& renderer );
	~ModelRenderable();

	ModelRenderable( const ModelRenderable& ) = delete;
	ModelRenderable& operator=( const ModelRenderable& ) = delete;
	ModelRenderable( ModelRenderable&& ) = delete;
	ModelRenderable& operator=( ModelRenderable&& ) = delete;

	VkResult Initialize();
	VkResult PrepareModel( ComputeCommandBuffer& computeCommandBuffer );

	void Update( const Camera& camera );
	void Render( GraphicsCommandBuffer& commandBuffer, const Camera& camera );
	void RenderDebug( GraphicsCommandBuffer& commandBuffer, const Camera& camera );
	void RenderNoDepthDebug( GraphicsCommandBuffer& commandBuffer, const Camera& camera, const DebugDrawSettings& settings );

	/// Sets the content that provides animations (and possibly an override skeleton). Resets the current animation.
	void SetAnimationOwner( std::shared_ptr<CmfContent> owner );
	/// Sets the animation to play, it must belong to the animation owner (nullptr shows the rest pose)
	void SetAnimation( const cmf::Animation* animation );
	/// Samples the current animation at the given time
	void SetAnimationTime( float animationTime );
	/// Applies an externally evaluated pose to the given skeleton of the model
	void SetPose( uint8_t skeletonIndex, const cmf::SkeletonPose& pose );

	void SetShader( const std::string& name, const GraphicsEffectTypes::ShaderInputDeclaration& declaration );
	void SetShowBoundingBox( bool show );
	void SetSelectedBones( const std::vector<uint32_t>& selectedBones );
	/// Forces a LOD for all meshes (clamped per mesh), -1 selects the LOD automatically from the size on screen
	void SetLodOverride( int32_t lod );
	void SetMorphWeightCallback( MorphWeightCallback callback );

	[[nodiscard]] size_t GetMeshCount() const;
	[[nodiscard]] MeshRenderable& GetMesh( size_t index );
	[[nodiscard]] const CcpMath::Sphere& GetBoundingSphere() const;
	[[nodiscard]] const std::shared_ptr<CmfContent>& GetContent() const;

private:
	void UpdateAnimation( float animationTime );

	std::vector<MeshRenderable> m_meshes;
	std::shared_ptr<const Renderer> m_renderer{ nullptr };
	CcpMath::Sphere m_boundingSphere{};
	std::shared_ptr<CmfContent> m_cmfContent{ nullptr };
	std::shared_ptr<CmfContent> m_animationOwner{ nullptr };
	int32_t m_lodOverride{ -1 };
	MorphWeightCallback m_morphWeightCallback;

	// perframe data
	struct PerObjectData
	{
		Matrix proj;
		Matrix view;
	};

	// one animation state per skeleton
	std::vector<AnimationState> m_animationStates;

	// bounding box debug
	bool m_showBoundingBox{ false };
	PrimitiveRenderable m_boundingBox;
	Matrix m_boundingBoxTransform{};
};
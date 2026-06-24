// Copyright © 2026 CCP ehf.

#pragma once

#include "../renderer.h"
#include "../vulkan/commandbuffer.h"
#include "mesh.h"
#include "animationState.h"


class ModelRenderable
{
public:
	ModelRenderable( std::shared_ptr<CmfContent> data, std::shared_ptr<const Renderer> renderer );
	~ModelRenderable();

	VkResult Initialize( AppState& appState );
	VkResult PrepareModel( ComputeCommandBuffer& computeCommandBuffer );

	void Update( AppState& appState, const Camera& camera );
	void Render( GraphicsCommandBuffer& commandBuffer, const AppState& appState, const Camera& camera );
	void RenderDebug( GraphicsCommandBuffer& commandBuffer, const AppState& appState, const Camera& camera );
	void RenderNoDepthDebug( GraphicsCommandBuffer& commandBuffer, const AppState& appState, const Camera& camera );

private:
	void UpdateAnimation( float animationTime, AppState& appState );

	std::vector<MeshRenderable> m_meshes{};
	std::shared_ptr<const Renderer> m_renderer{ nullptr };
	CcpMath::Sphere m_boundingSphere{};
	std::shared_ptr<CmfContent> m_cmfContent{ nullptr };

	// perframe data
	struct PerObjectData
	{
		Matrix proj;
		Matrix view;
	};

	// one animation state per skeleton
	std::vector<AnimationState> m_animationStates{};

	// bounding box debug
	bool m_showBoundingBox{ false };
	PrimitiveRenderable m_boundingBox;
	Matrix m_boundingBoxTransform{};
};
// Copyright © 2026 CCP ehf.

#pragma once

#include "appState.h"
#include "camera.h"
#include "data/cmfcontent.h"
#include "renderable/model.h"
#include "renderer.h"
#include "vulkan/commandbuffer.h"

class SceneRenderer
{
public:
	SceneRenderer( std::shared_ptr<Renderer> renderer );
	~SceneRenderer();

	VkResult Initialize( AppState& state );

	void Update( AppState& appState, const Camera& camera );
	void PrePass();
	void Render( const AppState& state, const Camera& camera );
	void SetData( std::shared_ptr<CmfContent> data, AppState& appState );

private:
	void ReleaseModel();

	GraphicsCommandBuffer m_depthLessDebugCommandBuffer;
	GraphicsCommandBuffer m_graphicsCommandBuffer;
	ComputeCommandBuffer m_computeCommandBuffer{};
	std::unique_ptr<ModelRenderable> m_model{ nullptr };
	std::shared_ptr<Renderer> m_renderer{ nullptr };
};

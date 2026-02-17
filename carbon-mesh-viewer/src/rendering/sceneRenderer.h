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
	SceneRenderer( std::shared_ptr<const Renderer> renderer );
	~SceneRenderer();

	VkResult Initialize( AppState& state );
	VkResult Render( const AppState& state, const Camera& camera );
	void SetData( CmfContent* data, AppState& appState );

private:
	void ReleaseModel();

	CommandBuffer m_commandBuffer;
	std::unique_ptr<ModelRenderable> m_model{ nullptr };
	std::shared_ptr<const Renderer> m_renderer{ nullptr };
};

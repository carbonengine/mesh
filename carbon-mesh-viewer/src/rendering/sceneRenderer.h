#pragma once

#include "appState.h"
#include "camera.h"
#include "data/cmfcontent.h"
#include "renderable/model.h"
#include "renderer.h"
#include "vulkan/commandbuffer.h"
#include "vulkan/shadercache.h"

class SceneRenderer
{
public:
	SceneRenderer( std::shared_ptr<const Renderer> renderer, std::shared_ptr<ShaderCache> shaderCache );
	~SceneRenderer();

	VkResult Initialize( AppState& state );
	VkResult Render( const AppState& state, const Camera& camera );
	void SetData( const CmfContent* data, const AppState& appstate );

private:
	void ReleaseModel();

	struct PerFrameData
	{
		Matrix proj;
		Matrix view;
	};

	CommandBuffer m_commandBuffer;

	std::unique_ptr<ModelRenderable> m_model{ nullptr };
	std::shared_ptr<const Renderer> m_renderer{ nullptr };
	std::shared_ptr<ShaderCache> m_shaderCache{ nullptr };
};

// Copyright © 2026 CCP ehf.

#pragma once

#include "camera.h"
#include "data/cmfcontent.h"
#include "renderable/debugDrawSettings.h"
#include "renderable/model.h"
#include "renderer.h"
#include "vulkan/commandbuffer.h"

#include "vulkan/rendertarget.h"

#include <optional>

class SceneRenderer
{
public:
	using ShaderEntry = std::pair<std::string, GraphicsEffectTypes::ShaderInputDeclaration>;
	using ShaderList = std::vector<ShaderEntry>;

	explicit SceneRenderer( const std::shared_ptr<Renderer>& renderer );
	~SceneRenderer();

	SceneRenderer( const SceneRenderer& ) = delete;
	SceneRenderer& operator=( const SceneRenderer& ) = delete;
	SceneRenderer( SceneRenderer&& ) = delete;
	SceneRenderer& operator=( SceneRenderer&& ) = delete;

	VkResult Initialize( uint32_t width, uint32_t height );

	void SetRenderSize( uint32_t width, uint32_t height );
	/// Renders into an offscreen target instead of the swapchain (nullptr restores the swapchain)
	void SetRenderTarget( const RenderTarget* target );
	void SetClearColor( float r, float g, float b );

	void Update( const Camera& camera );
	void PrePass();
	void Render( const Camera& camera, const DebugDrawSettings& debugSettings );

	/// Replaces the rendered model, returns the new model or nullptr when there is no data
	ModelRenderable* SetData( const std::shared_ptr<CmfContent>& data );
	[[nodiscard]] ModelRenderable* GetModel() const;

	/// Lists the visualization shaders that can render the given content
	static ShaderList GetAvailableShaders( const CmfContent& data );
	/// Returns the shader to switch to when the current shader is not in the available list
	static std::optional<ShaderEntry> ChooseShader( const ShaderList& shaders, const std::string& currentShaderName );

private:
	void ReleaseModel();

	GraphicsCommandBuffer m_depthLessDebugCommandBuffer;
	GraphicsCommandBuffer m_graphicsCommandBuffer;
	ComputeCommandBuffer m_computeCommandBuffer{};
	std::unique_ptr<ModelRenderable> m_model{ nullptr };
	std::shared_ptr<Renderer> m_renderer{ nullptr };
	const RenderTarget* m_renderTarget{ nullptr };
};

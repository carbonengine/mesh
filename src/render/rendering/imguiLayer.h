// Copyright © 2026 CCP ehf.

#pragma once

#include <memory>

#include <imgui.h>

#include "renderer.h"
#include "vulkan/commandbuffer.h"
#include "vulkan/rendertarget.h"

/// Owns the Dear ImGui GLFW/Vulkan backends and renders ImGui draw data into the swapchain.
///
/// Usage: CreateContext(), configure ImGui (fonts, style, settings), InitializeBackend(), then per frame
/// BeginFrame() ... build UI ... Render().
class ImGuiLayer
{
public:
	explicit ImGuiLayer( std::shared_ptr<const Renderer> renderer );
	~ImGuiLayer();

	ImGuiLayer( const ImGuiLayer& ) = delete;
	ImGuiLayer& operator=( const ImGuiLayer& ) = delete;
	ImGuiLayer( ImGuiLayer&& ) = delete;
	ImGuiLayer& operator=( ImGuiLayer&& ) = delete;

	/// Creates the ImGui context with keyboard navigation enabled
	static void CreateContext();

	/// Initializes the platform and renderer backends, the ImGui context must exist
	VkResult InitializeBackend( GLFWwindow* window, uint32_t width, uint32_t height );

	void SetRenderSize( uint32_t width, uint32_t height );
	/// Re-uploads the font atlas after it was rebuilt
	static void UpdateFontsTexture();

	static void BeginFrame();
	/// Finalizes the ImGui frame and records its draw commands into the current swapchain image
	void Render();

	/// Registers the color texture of a render target for use with ImGui::Image
	static ImTextureID AddTexture( const RenderTarget& target );
	static void RemoveTexture( ImTextureID texture );

private:
	std::shared_ptr<const Renderer> m_renderer;
	GraphicsCommandBuffer m_graphicsCommandBuffer;
	VkDescriptorPool m_descriptorPool{ VK_NULL_HANDLE };
	bool m_initialized{ false };
};

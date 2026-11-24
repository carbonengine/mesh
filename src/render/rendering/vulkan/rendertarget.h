// Copyright © 2026 CCP ehf.

#pragma once

#include <memory>

#include "texture.h"

class Renderer;

/// Offscreen color + depth target that can be rendered into and then sampled (e.g. shown in an ImGui window).
/// Uses the swapchain color format and the renderer depth format, so all pipelines created by the shader cache
/// are compatible with it.
class RenderTarget
{
public:
	explicit RenderTarget( std::shared_ptr<const Renderer> renderer );
	~RenderTarget();

	RenderTarget( const RenderTarget& ) = delete;
	RenderTarget& operator=( const RenderTarget& ) = delete;
	RenderTarget( RenderTarget&& ) = delete;
	RenderTarget& operator=( RenderTarget&& ) = delete;

	/// Recreates the target textures if the size changed. Waits for the device to be idle when it recreates.
	/// @return true if the textures were recreated (any external references to the image view must be refreshed)
	bool Resize( uint32_t width, uint32_t height );

	[[nodiscard]] bool IsValid() const;
	[[nodiscard]] uint32_t GetWidth() const;
	[[nodiscard]] uint32_t GetHeight() const;
	[[nodiscard]] const Texture* GetColorTexture() const;
	[[nodiscard]] const Texture* GetDepthTexture() const;
	[[nodiscard]] VkSampler GetSampler() const;

	/// Transitions the textures for rendering. Must be called on the current graphics command buffer before rendering.
	void BeginPass( VkCommandBuffer commandBuffer ) const;
	/// Transitions the color texture for sampling in fragment shaders.
	void EndPass( VkCommandBuffer commandBuffer ) const;

private:
	void Release();

	std::shared_ptr<const Renderer> m_renderer;
	std::unique_ptr<Texture> m_color;
	std::unique_ptr<Texture> m_depth;
	VkSampler m_sampler{ VK_NULL_HANDLE };
	uint32_t m_width{ 0 };
	uint32_t m_height{ 0 };
};

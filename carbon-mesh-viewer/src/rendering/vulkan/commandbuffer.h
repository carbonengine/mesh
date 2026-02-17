#pragma once

#include <cmf/cmf.h>

#include "../renderingConsts.h"
#include "../renderer.h"
#include "buffer.h"
#include "effect.h"

class CommandBuffer
{
public:
	CommandBuffer( const Renderer* renderer );

	void SetClearColor( float r, float g, float b );
	void SetClearDepth( float depth );

	void SetRenderSize( uint32_t width, uint32_t height );
	void SetRenderOffset( int32_t x, int32_t y );
	void SetLineWidth( float lineWidth );

	void Release( const Renderer* renderer );

	VkResult Begin( const Renderer* renderer );

	void BindEffect( Effect& effect );
	void Render( Buffer* vertexBuffer, Buffer* indexBuffer, uint32_t firstElement, uint32_t elementCount );
	VkResult End();

private:
	VkPipelineLayout m_pipelineLayout{ VK_NULL_HANDLE };
	VkDescriptorPool m_descriptorPool{ VK_NULL_HANDLE };

	uint32_t m_currentIndex{ 0 };

	VkExtent2D m_size{ 0, 0 };
	VkOffset2D m_offset{ 0, 0 };

	std::optional<Vector3> m_clearColor{ std::nullopt };
	std::optional<float> m_clearDepth{ std::nullopt };

	VkCommandBuffer m_activeCommandBuffer{ VK_NULL_HANDLE };

	// dynamic rendering function pointers
	PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR{ VK_NULL_HANDLE };
	PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR{ VK_NULL_HANDLE };
};

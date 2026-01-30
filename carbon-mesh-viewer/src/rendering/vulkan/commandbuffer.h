#pragma once

#include <cmf/cmf.h>

#include "../renderingConsts.h"
#include "../renderer.h"
#include "buffer.h"
#include "shadercache.h"

class CommandBuffer
{
public:
	CommandBuffer( const Renderer* renderer );

	void SetClearColor( float r, float g, float b );
	void SetClearDepth( float depth );

	void SetRenderSize( uint32_t width, uint32_t height );
	void SetRenderOffset( int32_t x, int32_t y );

	void Release( const Renderer* renderer );

	VkResult Begin( const Renderer* renderer );

	template <typename T>
	VkResult SetPerFrameData( const T& data )
	{
		return SetPerFrameData( &data, sizeof( T ) );
	}

	template <typename T>
	VkResult CreatePerFrameBuffers( const Renderer* renderer, const ShaderCache* shaderCache )
	{
		return CreatePerFrameBuffers( renderer, shaderCache, sizeof( T ) );
	}

	void BindPipeline( VkPipeline pipeline );
	void Render( Buffer* vertexBuffer, Buffer* indexBuffer, uint32_t firstElement, uint32_t elementCount );
	VkResult End();

private:
	VkResult CreatePerFrameBuffers( const Renderer* renderer, const ShaderCache* shaderCache, size_t perFrameDataSize );
	VkResult SetPerFrameData( const void* data, size_t size );

	struct UniformBuffer
	{
		VkDeviceMemory memory{ VK_NULL_HANDLE };
		VkBuffer buffer{ VK_NULL_HANDLE };
		VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
		uint8_t* mapped{ nullptr };
	};

	std::array<UniformBuffer, RenderingConsts::MAX_FRAMES_IN_FLIGHT> m_perFrameBuffers{};
	VkPipelineLayout m_pipelineLayout{ VK_NULL_HANDLE };
	VkDescriptorPool m_descriptorPool{ VK_NULL_HANDLE };

	size_t m_perFrameDataSize{ 0 };
	uint32_t m_currentIndex{ 0 };

	VkExtent2D m_size{ 0, 0 };
	VkOffset2D m_offset{ 0, 0 };

	VkViewport m_viewport{};
	VkRect2D m_scissor{};

	std::optional<Vector3> m_clearColor{ std::nullopt };
	std::optional<float> m_clearDepth{ std::nullopt };

	VkCommandBuffer m_activeCommandBuffer{ VK_NULL_HANDLE };

	// dynamic rendering function pointers
	PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR{ VK_NULL_HANDLE };
	PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR{ VK_NULL_HANDLE };
};

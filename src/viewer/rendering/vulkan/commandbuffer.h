// Copyright © 2026 CCP ehf.

#pragma once

#include "../renderingConsts.h"
#include "../renderer.h"
#include "buffer.h"
#include "effect.h"

class CommandBuffer
{
public:
	void BindEffect( Effect& effect );
	void BindVertexBuffer( VkBuffer buffer );
	void BindVertexBuffers( const std::vector<VkBuffer>& buffers );

	VkCommandBuffer GetActiveCommandBuffer() const;

protected:
	uint32_t m_currentIndex{ 0 };
	VkCommandBuffer m_activeCommandBuffer{ VK_NULL_HANDLE };
};


class GraphicsCommandBuffer : public CommandBuffer
{
public:
	GraphicsCommandBuffer( const Renderer* renderer );

	void Begin( const Renderer* renderer );
	void End();

	void SetClearColor( float r, float g, float b );
	void SetClearDepth( float depth );

	void SetRenderSize( uint32_t width, uint32_t height );
	void SetRenderOffset( int32_t x, int32_t y );
	void SetLineWidth( float lineWidth );

	void BindIndexBuffer( const Buffer& indexBuffer );

	void DrawIndexed( uint32_t firstElement, uint32_t elementCount, uint32_t instanceCount = 1 );
	void Draw( uint32_t firstElement, uint32_t elementCount, uint32_t instanceCount = 1 );

private:
	VkExtent2D m_size{ 0, 0 };
	VkOffset2D m_offset{ 0, 0 };

	std::optional<Vector3> m_clearColor{ std::nullopt };
	std::optional<float> m_clearDepth{ std::nullopt };
	// dynamic rendering function pointers
	PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR{ VK_NULL_HANDLE };
	PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR{ VK_NULL_HANDLE };
};

class ComputeCommandBuffer : public CommandBuffer
{
public:
	void Begin( const Renderer* renderer );
	void End();

	void Dispatch( uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ );

	void Copy( const Buffer& src, const Buffer& dst );
};
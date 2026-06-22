// Copyright © 2026 CCP ehf.

#include "commandbuffer.h"
#include "vulkanerrors.h"
#include "../renderingConsts.h"


void CommandBuffer::BindEffect( Effect& effect )
{
	effect.Bind( m_activeCommandBuffer, m_currentIndex );
}

void CommandBuffer::BindVertexBuffer( VkBuffer buffer )
{
	VkDeviceSize offset = { 0 };
	vkCmdBindVertexBuffers( m_activeCommandBuffer, 0, 1, &buffer, &offset );
}

void CommandBuffer::BindVertexBuffers( const std::vector<VkBuffer>& buffers )
{
	std::vector<VkDeviceSize> offsets;
	for( size_t i = 0; i < buffers.size(); i++ )
	{
		offsets.push_back( 0 );
	}
	vkCmdBindVertexBuffers( m_activeCommandBuffer, 0, static_cast<uint32_t>( buffers.size() ), buffers.data(), offsets.data() );
}

VkCommandBuffer CommandBuffer::GetActiveCommandBuffer() const
{
	return m_activeCommandBuffer;
}

// Graphics command buffer implementation
GraphicsCommandBuffer::GraphicsCommandBuffer( const Renderer* renderer ) :
	CommandBuffer()
{
	auto device = renderer->GetDevice()->GetLogicalDevice();
	vkCmdBeginRenderingKHR = reinterpret_cast<PFN_vkCmdBeginRenderingKHR>( vkGetDeviceProcAddr( device, "vkCmdBeginRenderingKHR" ) );
	vkCmdEndRenderingKHR = reinterpret_cast<PFN_vkCmdEndRenderingKHR>( vkGetDeviceProcAddr( device, "vkCmdEndRenderingKHR" ) );
}

void GraphicsCommandBuffer::SetClearColor( float r, float g, float b )
{
	m_clearColor = Vector3( r, g, b );
}

void GraphicsCommandBuffer::SetClearDepth( float depth )
{
	m_clearDepth = depth;
}

void GraphicsCommandBuffer::SetRenderSize( uint32_t width, uint32_t height )
{
	m_size = { width, height };
}

void GraphicsCommandBuffer::SetRenderOffset( int32_t x, int32_t y )
{
	m_offset = { x, y };
}

void GraphicsCommandBuffer::SetLineWidth( float lineWidth )
{
	vkCmdSetLineWidth( m_activeCommandBuffer, lineWidth );
}


void GraphicsCommandBuffer::BindIndexBuffer( const Buffer& indexBuffer )
{
	auto ib = indexBuffer.GetGpuBuffer();

	vkCmdBindIndexBuffer( m_activeCommandBuffer, ib, 0, indexBuffer.stride() == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32 );
}

void GraphicsCommandBuffer::Begin( const Renderer* renderer )
{
	if( vkCmdBeginRenderingKHR == nullptr || vkCmdEndRenderingKHR == nullptr )
	{
		Log::Error( "Dynamic rendering functions not loaded" );
		return;
	}

	// At this point, the current command buffer has been begun by the renderer
	m_activeCommandBuffer = renderer->GetCurrentGraphicVkCommandBuffer();
	m_currentIndex = renderer->GetCurrentFrame();

	auto swapchainFrameTexture = renderer->GetCurrentSwapchainFrameTexture();
	auto depthTexture = renderer->GetDepthTexture();

	VkRenderingAttachmentInfoKHR colorAttachment{};
	colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
	colorAttachment.imageView = swapchainFrameTexture->GetImageView();
	colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachment.loadOp = m_clearColor.has_value() ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	if( m_clearColor.has_value() )
	{
		auto& clearColor = m_clearColor.value();
		colorAttachment.clearValue.color = { { clearColor.x, clearColor.y, clearColor.z, 1.0f } };
	}

	VkRenderingAttachmentInfoKHR depthStencilAttachment{};
	depthStencilAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
	depthStencilAttachment.imageView = depthTexture->GetImageView();
	depthStencilAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depthStencilAttachment.loadOp = m_clearDepth.has_value() ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
	depthStencilAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	if( m_clearDepth.has_value() )
	{
		depthStencilAttachment.clearValue.depthStencil = { m_clearDepth.value(), 0 };
	}

	VkRenderingInfoKHR renderingInfo{};
	renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
	renderingInfo.renderArea = { m_offset.x, m_offset.y, m_size.width, m_size.height };
	renderingInfo.layerCount = 1;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments = &colorAttachment;
	renderingInfo.pDepthAttachment = &depthStencilAttachment;
	renderingInfo.pStencilAttachment = &depthStencilAttachment;

	// Begin dynamic rendering
	vkCmdBeginRenderingKHR( m_activeCommandBuffer, &renderingInfo );

	VkViewport viewport{};
	viewport.width = (float)m_size.width;
	viewport.height = -(float)m_size.height;
	viewport.x = (float)m_offset.x;
	viewport.y = (float)m_size.height + (float)m_offset.y;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport( m_activeCommandBuffer, 0, 1, &viewport );

	VkRect2D scissor{};
	scissor.offset = m_offset;
	scissor.extent.width = m_size.width;
	scissor.extent.height = m_size.height;

	vkCmdSetScissor( m_activeCommandBuffer, 0, 1, &scissor );
}

void GraphicsCommandBuffer::End()
{
	vkCmdEndRenderingKHR( m_activeCommandBuffer );
	m_activeCommandBuffer = VK_NULL_HANDLE;
}

void GraphicsCommandBuffer::DrawIndexed( uint32_t firstElement, uint32_t elementCount, uint32_t instanceCount )
{
	vkCmdDrawIndexed( m_activeCommandBuffer, elementCount, instanceCount, firstElement, 0, 0 );
}

void GraphicsCommandBuffer::Draw( uint32_t firstElement, uint32_t elementCount, uint32_t instanceCount )
{
	vkCmdDraw( m_activeCommandBuffer, elementCount, instanceCount, firstElement, 0 );
}

// Compute command buffer implementation
void ComputeCommandBuffer::Begin( const Renderer* renderer )
{
	m_activeCommandBuffer = renderer->GetCurrentComputeVkCommandBuffer();
}

void ComputeCommandBuffer::End()
{
	m_activeCommandBuffer = VK_NULL_HANDLE;
}

void ComputeCommandBuffer::Dispatch( uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ )
{
	vkCmdDispatch( m_activeCommandBuffer, groupCountX, groupCountY, groupCountZ );
}

void ComputeCommandBuffer::Copy( const Buffer& src, const Buffer& dst )
{
	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = std::min( src.size(), dst.size() );
	vkCmdCopyBuffer( m_activeCommandBuffer, src.GetGpuBuffer(), dst.GetGpuBuffer(), 1, &copyRegion );
}
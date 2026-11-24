// Copyright © 2026 CCP ehf.

#include "rendertarget.h"

#include "../renderer.h"
#include "vulkanerrors.h"

namespace
{
constexpr VkFormat DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT_S8_UINT;

void ImageBarrier( VkCommandBuffer commandBuffer, VkImage image, VkImageAspectFlags aspect, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage )
{
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.srcAccessMask = srcAccess;
	barrier.dstAccessMask = dstAccess;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange = VkImageSubresourceRange{ aspect, 0, 1, 0, 1 };
	vkCmdPipelineBarrier( commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier );
}
}

RenderTarget::RenderTarget( std::shared_ptr<const Renderer> renderer ) :
	m_renderer( std::move( renderer ) )
{
}

RenderTarget::~RenderTarget()
{
	if( m_color || m_depth )
	{
		vkDeviceWaitIdle( m_renderer->GetDevice()->GetLogicalDevice() );
	}
	Release();
	if( m_sampler != VK_NULL_HANDLE )
	{
		vkDestroySampler( m_renderer->GetDevice()->GetLogicalDevice(), m_sampler, m_renderer->GetAllocator() );
	}
}

void RenderTarget::Release()
{
	Device* device = m_renderer->GetDevice();
	if( m_color )
	{
		m_color->Release( device );
		m_color.reset();
	}
	if( m_depth )
	{
		m_depth->Release( device );
		m_depth.reset();
	}
}

bool RenderTarget::Resize( uint32_t width, uint32_t height )
{
	width = std::max( width, 1u );
	height = std::max( height, 1u );
	if( width == m_width && height == m_height && IsValid() )
	{
		return false;
	}

	Device* device = m_renderer->GetDevice();
	if( m_color || m_depth )
	{
		vkDeviceWaitIdle( device->GetLogicalDevice() );
	}
	Release();

	if( m_sampler == VK_NULL_HANDLE )
	{
		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.maxLod = 1.0f;
		if( vkCreateSampler( device->GetLogicalDevice(), &samplerInfo, m_renderer->GetAllocator(), &m_sampler ) != VK_SUCCESS )
		{
			Log::Error( "Failed to create render target sampler" );
			return false;
		}
	}

	const VkFormat colorFormat = m_renderer->GetSwapchain()->GetFormat();
	m_color.reset( Texture::Create( device, width, height, colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT ) );
	m_depth.reset( Texture::Create( device, width, height, DEPTH_FORMAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT ) );
	if( !m_color || !m_depth )
	{
		Log::Error( "Failed to create render target textures (%ux%u)", width, height );
		Release();
		m_width = 0;
		m_height = 0;
		return false;
	}
	m_width = width;
	m_height = height;
	return true;
}

bool RenderTarget::IsValid() const
{
	return m_color != nullptr && m_depth != nullptr;
}

uint32_t RenderTarget::GetWidth() const
{
	return m_width;
}

uint32_t RenderTarget::GetHeight() const
{
	return m_height;
}

const Texture* RenderTarget::GetColorTexture() const
{
	return m_color.get();
}

const Texture* RenderTarget::GetDepthTexture() const
{
	return m_depth.get();
}

VkSampler RenderTarget::GetSampler() const
{
	return m_sampler;
}

void RenderTarget::BeginPass( VkCommandBuffer commandBuffer ) const
{
	if( !IsValid() )
	{
		return;
	}
	// the previous contents are always cleared, so the old layout can be undefined
	ImageBarrier( commandBuffer, m_color->GetImage(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT );
	ImageBarrier( commandBuffer, m_depth->GetImage(), VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT );
}

void RenderTarget::EndPass( VkCommandBuffer commandBuffer ) const
{
	if( !IsValid() )
	{
		return;
	}
	ImageBarrier( commandBuffer, m_color->GetImage(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
}

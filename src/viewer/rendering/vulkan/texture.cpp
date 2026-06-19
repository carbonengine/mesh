// Copyright © 2025 CCP ehf.

#include "texture.h"

Texture::Texture() :
	m_image( VK_NULL_HANDLE ),
	m_imageMemory( VK_NULL_HANDLE ),
	m_imageView( VK_NULL_HANDLE )
{
}

Texture::Texture( VkImage image, VkDeviceMemory imageMemory, VkImageView imageView ) :
	m_image( image ),
	m_imageMemory( imageMemory ),
	m_imageView( imageView )
{
}


Texture::~Texture()
{
}

Texture* Texture::Create( Device* device, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspectFlags )
{
	Texture* texture = new Texture();
	texture->m_format = format;

	VkDevice logicalDevice = device->GetLogicalDevice();
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if( vkCreateImage( logicalDevice, &imageInfo, nullptr, &texture->m_image ) != VK_SUCCESS )
	{
		return nullptr;
	}

	texture->m_needsImageClearing = true;

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements( logicalDevice, texture->m_image, &memRequirements );

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = Texture::FindMemoryType( memRequirements.memoryTypeBits, device->GetPhysicalDevice() );

	if( vkAllocateMemory( logicalDevice, &allocInfo, nullptr, &texture->m_imageMemory ) != VK_SUCCESS )
	{
		texture->Release( device );
		return nullptr;
	}

	if( vkBindImageMemory( logicalDevice, texture->m_image, texture->m_imageMemory, 0 ) != VK_SUCCESS )
	{
		texture->Release( device );
		return nullptr;
	}

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = texture->m_image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = aspectFlags;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	if( vkCreateImageView( logicalDevice, &viewInfo, nullptr, &texture->m_imageView ) != VK_SUCCESS )
	{
		texture->Release( device );
		return nullptr;
	}

	return texture;
}

Texture* Texture::CreateFromImage( Device* device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags )
{
	Texture* texture = new Texture();
	texture->m_image = image;
	texture->m_format = format;

	auto logicalDevice = device->GetLogicalDevice();
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = aspectFlags;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	if( vkCreateImageView( logicalDevice, &viewInfo, nullptr, &texture->m_imageView ) != VK_SUCCESS )
	{
		texture->Release( device );
		return nullptr;
	}

	return texture;
}

uint32_t Texture::FindMemoryType( uint32_t typeFilter, VkPhysicalDevice physicalDevice )
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties( physicalDevice, &memProperties );

	for( uint32_t i = 0; i < memProperties.memoryTypeCount; i++ )
	{
		if( ( typeFilter & ( 1 << i ) ) && ( memProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT )
		{
			return i;
		}
	}

	throw std::runtime_error( "failed to find suitable memory type!" );
}

void Texture::Release( Device* device )
{
	VkDevice logicalDevice = device->GetLogicalDevice();
	if( m_imageView != VK_NULL_HANDLE )
	{
		vkDestroyImageView( logicalDevice, m_imageView, nullptr );
		m_imageView = VK_NULL_HANDLE;
	}
	if( m_image != VK_NULL_HANDLE && m_needsImageClearing )
	{
		vkDestroyImage( logicalDevice, m_image, nullptr );
		m_image = VK_NULL_HANDLE;
	}
	if( m_imageMemory != VK_NULL_HANDLE )
	{
		vkFreeMemory( logicalDevice, m_imageMemory, nullptr );
		m_imageMemory = VK_NULL_HANDLE;
	};
}

VkImage Texture::GetImage() const
{
	return m_image;
}

VkImageView Texture::GetImageView() const
{
	return m_imageView;
}

VkFormat Texture::GetFormat() const
{
	return m_format;
}
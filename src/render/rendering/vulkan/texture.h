// Copyright © 2025 CCP ehf.

#pragma once

#include "device.h"

// Handles creating an destroying a textures image, view and memory
class Texture
{
public:
	Texture();
	Texture( VkImage image, VkDeviceMemory imageMemory, VkImageView imageView );
	~Texture();

	static Texture* Create( Device* device, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspectFlags );
	static Texture* CreateFromImage( Device* device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags );
	void Release( Device* device );

	VkImage GetImage() const;
	VkImageView GetImageView() const;
	VkFormat GetFormat() const;

private:
	VkImage m_image{ VK_NULL_HANDLE };
	VkDeviceMemory m_imageMemory{ VK_NULL_HANDLE };
	VkImageView m_imageView{ VK_NULL_HANDLE };
	VkFormat m_format{ VK_FORMAT_UNDEFINED };
	static uint32_t FindMemoryType( uint32_t typeFilter, VkPhysicalDevice physicalDevice );
	bool m_needsImageClearing{ false };
};
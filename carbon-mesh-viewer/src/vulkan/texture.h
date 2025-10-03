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

private:
	VkImage m_image;
	VkDeviceMemory m_imageMemory;
	VkImageView m_imageView;
	static uint32_t FindMemoryType( uint32_t typeFilter, VkPhysicalDevice physicalDevice );
};
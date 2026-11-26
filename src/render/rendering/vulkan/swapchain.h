// Copyright © 2025 CCP ehf.

#pragma once

#include "device.h"
#include "texture.h"


// Swapchain class which handles creating and destroying the swapchain and its related resources
class Swapchain
{
public:
	Swapchain();
	~Swapchain();
	VkResult Release( Device* device, const VkAllocationCallbacks* allocator );
	VkResult Initialize( Device* device, VkSurfaceKHR surface, const VkAllocationCallbacks* allocator, uint32_t width, uint32_t height );
	VkFormat GetFormat() const;
	VkSwapchainKHR GetVulkanSwapchain() const;
	VkFramebuffer GetFrameBuffer( size_t index ) const;
	VkExtent2D GetExtent() const;

	const Texture* GetFrameTexture( size_t index ) const;
	uint32_t GetImageCount() const;
	uint32_t GetMinImageCount() const;

private:
	VkSwapchainKHR m_swapchain;
	VkFormat m_swapchainImageFormat;
	VkExtent2D m_swapchainExtent;
	uint32_t m_minImageCount;
	std::vector<VkImage> m_swapchainImages{};
	std::vector<Texture*> m_swapchainFrames;
	std::vector<VkFramebuffer> m_swapchainFramebuffers;

	VkResult CreateVulkanSwapchain( Device* device, VkSurfaceKHR surface, const VkAllocationCallbacks* allocator );

	VkSurfaceFormatKHR ChooseSwapSurfaceFormat( const std::vector<VkSurfaceFormatKHR>& availableFormats );
	VkPresentModeKHR ChooseSwapPresentMode( const std::vector<VkPresentModeKHR>& availablePresentModes );
};
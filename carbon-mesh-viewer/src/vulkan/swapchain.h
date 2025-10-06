#pragma once
#include "device.h"
#include "texture.h"


class Swapchain
{
public:
	Swapchain();
	~Swapchain();
	VkResult Release( Device* device, const VkAllocationCallbacks* allocator );
	VkResult Initialize( Device* device, VkSurfaceKHR surface, const VkAllocationCallbacks* allocator, uint32_t width, uint32_t height );
	VkFormat GetFormat() const;
	VkResult CreateFrameBuffers( Device* device, VkRenderPass renderPass, Texture* depth );
	VkSwapchainKHR GetVulkanSwapchain() const;
	VkFramebuffer GetFrameBuffer( size_t index ) const;
	VkExtent2D GetExtent() const;

	size_t GetImageCount() const;

private:
	VkSwapchainKHR m_swapchain;
	VkFormat m_swapchainImageFormat;
	VkExtent2D m_swapchainExtent;
	std::vector<Texture*> m_swapchainFrames;
	std::vector<VkFramebuffer> m_swapchainFramebuffers;

	VkResult CreateVulkanSwapchain( Device* device, VkSurfaceKHR surface, const VkAllocationCallbacks* allocator );

	VkSurfaceFormatKHR ChooseSwapSurfaceFormat( const std::vector<VkSurfaceFormatKHR>& availableFormats );
	VkPresentModeKHR ChooseSwapPresentMode( const std::vector<VkPresentModeKHR>& availablePresentModes );
};
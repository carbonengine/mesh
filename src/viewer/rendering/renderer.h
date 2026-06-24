// Copyright © 2025 CCP ehf.

#pragma once

#include "appState.h"
#include "renderingConsts.h"
#include "vulkan/device.h"
#include "vulkan/swapchain.h"
#include "vulkan/texture.h"

namespace
{
static VKAPI_ATTR VkBool32 VKAPI_CALL validationCallback( VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData );
}

// Handles the boilerplate vulkan setup and begin/end of rendering
class Renderer
{
public:
	~Renderer();

	VkResult CreateInstance( std::vector<const char*> extensions );

	void Initialize();
	void PreResize();
	VkResult Resize( uint32_t width, uint32_t height );
	void ReleaseSurface();

	VkResult BeginRender();
	VkResult EndRender();

	VkResult BeginCompute();
	VkResult EndCompute();

	bool IsValid() const;

	VkInstance GetVulkanInstance() const;
	VkCommandBuffer GetCurrentGraphicVkCommandBuffer() const;
	VkCommandBuffer GetCurrentComputeVkCommandBuffer() const;

	const Texture* GetCurrentSwapchainFrameTexture() const;
	const Texture* GetDepthTexture() const;
	void CreateFrameFence() const;
	void CreateDepthFence() const;

	VkResult CreateCopyCommandBuffer( VkCommandBuffer* commandBuffer ) const;
	VkResult EndCopyCommandBuffer( VkCommandBuffer commandBuffer ) const;

	VkAllocationCallbacks* GetAllocator() const;
	Device* GetDevice() const;
	VkCommandPool GetCommandPool() const;
	uint32_t GetCurrentFrame() const;
	VkDescriptorPool GetDescriptorPool() const;

	const Swapchain* GetSwapchain() const;

	uint32_t GetWidth() const;
	uint32_t GetHeight() const;

	VkSurfaceKHR* GetSurface();

	bool HasDedicatedComputeQueue() const;

private:
	uint32_t m_imageIndex;

	void PresentFence() const;
	VkResult CreateCommandPool();
	VkResult CreateSyncObjects();
	VkResult CreateDescriptorPool();
	VkResult CreateCommandBuffers();

	VkInstance m_instance{ VK_NULL_HANDLE };
	VkSurfaceKHR m_surface{ VK_NULL_HANDLE };
	Swapchain* m_swapchain{ nullptr };
	Device* m_device{ nullptr };
	VkAllocationCallbacks* m_allocator{ nullptr };
	VkDescriptorPool m_descriptorPool{ VK_NULL_HANDLE };
	uint32_t m_currentFrame{ 0 };
	uint32_t m_currentSemaphore{ 0 };
	uint32_t m_lastSemaphore{ 0 };

	Texture* m_depthTarget{ nullptr };

	VkCommandPool m_graphicsCommandPool{ VK_NULL_HANDLE };
	std::array<VkCommandBuffer, RenderingConsts::MAX_FRAMES_IN_FLIGHT> m_graphicsCommandBuffers{};

	VkCommandPool m_computeCommandPool{ VK_NULL_HANDLE };
	std::array<VkCommandBuffer, RenderingConsts::MAX_FRAMES_IN_FLIGHT> m_computeCommandBuffers{};

	// fences and semaphores
	std::vector<VkSemaphore> m_imageAvailableSemaphores;
	std::vector<VkSemaphore> m_renderFinishedSemaphores;
	std::vector<VkSemaphore> m_computeFinishedSemaphores;
	std::vector<VkSemaphore> m_computeReadySemaphores;
	std::vector<VkFence> m_inFlightGraphicsFence;
	std::vector<VkFence> m_inFlightComputeFence;

	uint32_t m_width{ 0 };
	uint32_t m_height{ 0 };

	bool m_valid;
};
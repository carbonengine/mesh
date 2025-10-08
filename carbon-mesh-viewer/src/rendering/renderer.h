#pragma once

#include "vulkan/device.h"
#include "vulkan/swapchain.h"
#include "vulkan/texture.h"


namespace RenderUtils
{
static VKAPI_ATTR VkBool32 VKAPI_CALL validationCallback( VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData );
static const int MAX_FRAMES_IN_FLIGHT = 2;
}

// handles all vulkan related tasks
class Renderer
{
public:
	Renderer();
	~Renderer();

	VkResult CreateInstance( std::vector<const char*> extensions );

	void Initialize();
	void PreResize();
	VkResult Resize( uint32_t width, uint32_t height );
	void ReleaseSurface();

	VkResult BeginRender();
	VkResult EndRender();

	bool IsValid() const;

	VkInstance GetVulkanInstance() const;
	VkRenderPass GetRenderPass() const;
	VkCommandBuffer GetCurrentCommandBuffer() const;
	VkAllocationCallbacks* GetAllocator() const;
	Device* GetDevice() const;
	VkCommandPool GetCommandPool() const;
	uint32_t GetCurrentFrame() const;
	VkDescriptorPool GetDescriptorPool() const;

	uint32_t GetWidth() const;
	uint32_t GetHeight() const;

	VkSurfaceKHR* GetSurface();

private:
	uint32_t m_imageIndex;

	VkResult CreateRenderPass();
	VkResult CreateCommandBuffers();
	VkResult CreateSyncObjects();
	VkResult CreateDescriptorPool();

	VkInstance m_instance;
	VkSurfaceKHR m_surface;
	VkRenderPass m_renderPass;
	Swapchain* m_swapchain;
	Device* m_device;
	VkAllocationCallbacks* m_allocator;
	VkDescriptorPool m_descriptorPool;
	uint32_t m_currentFrame;
	uint32_t m_currentSemaphore;

	Texture* m_depthTarget;
	VkCommandPool m_commandPool;

	std::vector<VkCommandBuffer> m_commandBuffers;

	// fences and semaphores
	std::vector<VkSemaphore> m_imageAvailableSemaphores;
	std::vector<VkSemaphore> m_renderFinishedSemaphores;
	std::vector<VkFence> m_inFlightFences;


	uint32_t m_width;
	uint32_t m_height;
	float m_rot;
	VkPipeline m_pipeline;

	bool m_valid;
};
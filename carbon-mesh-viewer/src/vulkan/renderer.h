#pragma once
#include "device.h"
#include "swapchain.h"
#include "texture.h"
#include "model/Model.h"

namespace RenderUtils
{
static VKAPI_ATTR VkBool32 VKAPI_CALL validationCallback( VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData );
static const int MAX_FRAMES_IN_FLIGHT = 2;
}


class Renderer
{
public:
	Renderer();
	~Renderer();

	VkResult CreateInstance( std::vector<const char*> extensions );
	VkResult init( uint32_t width, uint32_t height, VkSurfaceKHR surface );
	VkResult RenderFrame();
	void Update( float dt );

	void SetModel( Model* Model );

	VkInstance GetVulkanInstance() const;

private:
	struct UniformBuffer
	{
		VkDeviceMemory memory{ VK_NULL_HANDLE };
		VkBuffer buffer{ VK_NULL_HANDLE };
		// The descriptor set stores the resources bound to the binding points in a shader
		// It connects the binding points of the different shaders with the buffers and images used for those bindings
		VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
		// We keep a pointer to the mapped buffer, so we can easily update it's contents via a memcpy
		uint8_t* mapped{ nullptr };
	};

	struct PerFrameData
	{
		Matrix proj;
		Matrix view;
	};

	struct Camera
	{
		Vector3 at;
		Vector3 pos;
		Vector3 up;
		Matrix translation;
	};

	VkResult CreateRenderPass();
	VkResult CreateCommandBuffers();
	VkResult CreateSyncObjects();
	VkResult CreatePerFrameData();
	VkResult CreateDescriptorSetLayout();
	VkResult CreateDescriptorPool();
	VkResult CreateDescriptorSets();
	VkResult RecordCommandBuffer( VkCommandBuffer commandBuffer, uint32_t imageIndex );

	Camera m_camera;

	VkInstance m_instance;
	VkSurfaceKHR m_surface;
	VkRenderPass m_renderPass;
	Swapchain* m_swapchain;
	Device* m_device;
	VkAllocationCallbacks* m_allocator;
	VkDescriptorPool m_descriptorPool;
	VkDescriptorSetLayout m_descriptorSetLayout;
	uint32_t m_currentFrame;
	uint32_t m_currentSemaphore;

	Texture* m_depthTarget;
	VkCommandPool m_commandPool;

	std::vector<VkCommandBuffer> m_commandBuffers;

	// fences and semaphores
	std::vector<VkSemaphore> m_imageAvailableSemaphores;
	std::vector<VkSemaphore> m_renderFinishedSemaphores;
	std::vector<VkFence> m_inFlightFences;

	std::array<UniformBuffer, RenderUtils::MAX_FRAMES_IN_FLIGHT> m_perFrameBuffers;

	uint32_t m_width;
	uint32_t m_height;
	float m_rot;
	Model* m_model;
};

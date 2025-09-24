#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

class RenderContext
{
public:
	RenderContext();
	~RenderContext();

	VkResult Init( GLFWwindow* window );

	VkResult Render();
	void Update(float dt);
	VkResult Resize();


private:
	void Cleanup();
	void BeginFrame();
	void RenderMesh();
	void EndFrame();
	void DrawUI();

	VkResult CreateInstance();
	VkResult SetupDebugMessenger();
	VkResult CreateSurface( GLFWwindow* window );

	
	VkAllocationCallbacks* m_allocator;
	VkInstance m_instance;
	VkPhysicalDevice m_physicalDevice;
	VkDevice m_device;
	uint32_t m_queueFamily;
	VkQueue m_queue;
	VkPipelineCache m_pipelineCache;
	VkDescriptorPool m_descriptorPool;
	VkSurfaceKHR m_surface;

};
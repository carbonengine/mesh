#include "renderer.h"

#include "vulkan/device.h"
#include <fstream>
#include <stdexcept>
#include "vulkan/shadercache.h"

namespace RenderUtils
{
VKAPI_ATTR VkBool32 VKAPI_CALL validationCallback( VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData )
{
	switch( messageSeverity )
	{
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
		CCP_LOGNOTICE( "[vulkan] validation layer: VERBOSE: %s", pCallbackData->pMessage );
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
		CCP_LOGNOTICE( "[vulkan] validation layer: INFO: %s", pCallbackData->pMessage );
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
		CCP_LOGWARN( "[vulkan] validation layer: WARNING: %s", pCallbackData->pMessage );
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
		CCP_LOGERR( "[vulkan] validation layer: ERROR: %s", pCallbackData->pMessage );
		break;
	default:
		CCP_LOGERR( "[vulkan] validation layer: UNKNOWN: %s", pCallbackData->pMessage );
		break;
	}

	return VK_FALSE;
}
}

Renderer::Renderer() 
{
}

Renderer::~Renderer()
{
	if( m_instance != VK_NULL_HANDLE )
	{
		VkDevice logicalDevice = m_device->GetLogicalDevice();
		if( m_renderPass != VK_NULL_HANDLE )
		{
			vkDestroyRenderPass( logicalDevice, m_renderPass, m_allocator );
		}
		if( m_descriptorPool != VK_NULL_HANDLE )
		{
			vkDestroyDescriptorPool( logicalDevice, m_descriptorPool, m_allocator );
		}

		if( m_depthTarget )
		{
			m_depthTarget->Release( m_device );
			delete m_depthTarget;
			m_depthTarget = nullptr;
		}
		if( m_swapchain )
		{
			m_swapchain->Release( m_device, m_allocator );
			delete m_swapchain;
			m_swapchain = nullptr;
		}

		vkDestroyCommandPool( logicalDevice, m_commandPool, m_allocator );
		for( auto& semaphore : m_renderFinishedSemaphores )
		{
			vkDestroySemaphore( logicalDevice, semaphore, m_allocator );
		}
		for( auto& semaphore : m_imageAvailableSemaphores )
		{
			vkDestroySemaphore( logicalDevice, semaphore, m_allocator );
		}
		for( uint32_t i = 0; i < RenderUtils::MAX_FRAMES_IN_FLIGHT; ++i )
		{
			vkDestroyFence( logicalDevice, m_inFlightFences[i], m_allocator );
			//vkDestroyBuffer( logicalDevice, m_perFrameBuffers[i].buffer, m_allocator );
			//vkFreeMemory( logicalDevice, m_perFrameBuffers[i].memory, m_allocator );
		}
		vkDestroyInstance( m_instance, m_allocator );
	}
}

bool Renderer::IsValid() const
{
	return m_valid;
}

VkResult Renderer::CreateInstance( std::vector<const char*> extensions )
{
	// should we create an allocator???

#ifdef DEBUG_MODE
	extensions.push_back( VK_EXT_DEBUG_UTILS_EXTENSION_NAME );
	extensions.push_back( VK_EXT_DEBUG_REPORT_EXTENSION_NAME );
#endif

	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "CarbonMeshViewer";
	appInfo.applicationVersion = VK_MAKE_VERSION( 1, 0, 0 );
	appInfo.pEngineName = "Stalled Engine";
	appInfo.engineVersion = VK_MAKE_VERSION( 1, 0, 0 );
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	createInfo.enabledExtensionCount = (uint32_t)extensions.size();
	createInfo.ppEnabledExtensionNames = extensions.data();

#ifdef DEBUG_MODE
	const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
	createInfo.enabledLayerCount = 1;
	createInfo.ppEnabledLayerNames = layers;
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
	debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debugCreateInfo.pfnUserCallback = RenderUtils::validationCallback;
	createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
#else
	createInfo.enabledLayerCount = 0;
	createInfo.pNext = nullptr;
#endif

	RETURN_ERROR( CR( vkCreateInstance( &createInfo, m_allocator, &m_instance ) ) );

	return VK_SUCCESS;
}

void Renderer::Initialize()
{
	// Initialize device
	m_device = new Device();
	ON_ERROR_LOG_AND_RETURN( m_device->Initialize( m_instance, m_allocator, m_surface ), "Could not initialize device" );

	m_swapchain = new Swapchain();

	ON_ERROR_LOG_AND_RETURN( m_swapchain->Initialize( m_device, m_surface, m_allocator, m_width, m_height ), "Could not initialize swapchain" );
	m_depthTarget = Texture::Create( m_device, m_width, m_height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT );

	ON_ERROR_LOG_AND_RETURN( CreateRenderPass(), "Could not create render pass" );
	ON_ERROR_LOG_AND_RETURN( m_swapchain->CreateFrameBuffers( m_device, m_renderPass, m_depthTarget ), "Could not create frame buffers" );
	ON_ERROR_LOG_AND_RETURN( CreateCommandBuffers(), "Could not create command buffers" );
	ON_ERROR_LOG_AND_RETURN( CreateSyncObjects(), "Could not create sync objects" );
	ON_ERROR_LOG_AND_RETURN( CreateDescriptorPool(), "Could not cretae descriptor pool" );

	m_valid = true;
}

void Renderer::PreResize()
{
	vkDeviceWaitIdle( m_device->GetLogicalDevice() );
	if( m_depthTarget )
	{
		m_depthTarget->Release( m_device );
		delete m_depthTarget;
		m_depthTarget = nullptr;
	}
	if( m_swapchain )
	{
		m_swapchain->Release( m_device, m_allocator );
		delete m_swapchain;
		m_swapchain = nullptr;
	}
}

void Renderer::ReleaseSurface()
{
	vkDestroySurfaceKHR( m_instance, m_surface, m_allocator );
}


VkResult Renderer::Resize( uint32_t width, uint32_t height )
{
	if( width == 0 || height == 0 )
	{
		return VK_SUCCESS;
	}
	m_width = width;
	m_height = height;
	if( m_device == nullptr )
	{
		return VK_SUCCESS;
	}
	m_depthTarget = Texture::Create( m_device, m_width, m_height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT );

	m_swapchain = new Swapchain();
	RETURN_ERROR( m_swapchain->Initialize( m_device, m_surface, m_allocator, m_width, m_height ) );
	RETURN_ERROR( CreateRenderPass() );
	RETURN_ERROR( m_swapchain->CreateFrameBuffers( m_device, m_renderPass, m_depthTarget ) );

	return VK_SUCCESS;
}


VkResult Renderer::BeginRender()
{
	VkDevice device = m_device->GetLogicalDevice();

	vkWaitForFences( device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX );
	CR( vkResetFences( device, 1, &m_inFlightFences[m_currentFrame] ) );

	VkResult result = vkAcquireNextImageKHR( device,
											 m_swapchain->GetVulkanSwapchain(),
											 UINT64_MAX,
											 m_imageAvailableSemaphores[m_currentSemaphore],
											 VK_NULL_HANDLE,
											 &m_imageIndex );

	if( result == VK_ERROR_OUT_OF_DATE_KHR )
	{
		//throw std::runtime_error( "Swap chain is out of date!" );
		Resize( m_width, m_height );
		return result;
	}
	else if( result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR )
	{
		throw std::runtime_error( "failed to acquire swap chain image!" );
	}

	auto commandBuffer = m_commandBuffers[m_currentFrame];

	CR_RETURN( vkResetCommandBuffer( commandBuffer, /*VkCommandBufferResetFlagBits*/ 0 ) );

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	CR_RETURN( vkBeginCommandBuffer( commandBuffer, &beginInfo ) );

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = m_renderPass;
	renderPassInfo.framebuffer = m_swapchain->GetFrameBuffer( m_imageIndex );
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = m_swapchain->GetExtent();

	std::array<VkClearValue, 2> clearValues{};

	clearValues[0].color = { { 0.1f, 0.1f, 0.1f, 1.0f } };
	clearValues[1].depthStencil = { 1.0f, 0 };

	renderPassInfo.clearValueCount = static_cast<uint32_t>( clearValues.size() );
	renderPassInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass( commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE );

	VkViewport viewport{};
	viewport.width = (float)m_width;
	viewport.height = -(float)m_height;
	viewport.x = 0;
	viewport.y = (float)m_height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport( commandBuffer, 0, 1, &viewport );

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent.width = m_width;
	scissor.extent.height = m_height;

	vkCmdSetScissor( commandBuffer, 0, 1, &scissor );

	return VK_SUCCESS;
}

VkResult Renderer::EndRender()
{
	auto commandBuffer = m_commandBuffers[m_currentFrame];

	vkCmdEndRenderPass( commandBuffer );

	CR_RETURN( vkEndCommandBuffer( commandBuffer ) );

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &m_imageAvailableSemaphores[m_currentSemaphore];
	submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_commandBuffers[m_currentFrame];

	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &m_renderFinishedSemaphores[m_currentSemaphore];

	CR_RETURN( vkQueueSubmit( m_device->GetGraphicsQueue(), 1, &submitInfo, m_inFlightFences[m_currentFrame] ) );

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &m_renderFinishedSemaphores[m_currentSemaphore];

	VkSwapchainKHR swapChains[] = { m_swapchain->GetVulkanSwapchain() };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;

	presentInfo.pImageIndices = &m_imageIndex;

	auto result = vkQueuePresentKHR( m_device->GetPresentQueue(), &presentInfo );

	if( result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ) // || framebufferResized )
	{
		PreResize();
		Resize( m_width, m_height );
		return VK_SUCCESS;
	}
	else if( result != VK_SUCCESS )
	{
		throw std::runtime_error( "failed to present swap chain image!" );
	}

	m_currentFrame = ( m_currentFrame + 1 ) % RenderUtils::MAX_FRAMES_IN_FLIGHT;
	m_currentSemaphore = ( m_currentSemaphore + 1 ) % m_swapchain->GetImageCount();
	return VK_SUCCESS;
}

VkResult Renderer::CreateRenderPass()
{
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = m_swapchain->GetFormat();
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentDescription depthAttachment{};
	depthAttachment.format = VK_FORMAT_D32_SFLOAT; // todo - find a format that works
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef{};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>( attachments.size() );
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	RETURN_LOG_ERROR( vkCreateRenderPass( m_device->GetLogicalDevice(), &renderPassInfo, nullptr, &m_renderPass ), "Failed to create render pass" );
	return VK_SUCCESS;
}

VkResult Renderer::CreateCommandBuffers()
{
	QueueFamilyIndices queueFamilyIndices = m_device->GetQueueFamilyIndices();
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

	CR_RETURN( vkCreateCommandPool( m_device->GetLogicalDevice(), &poolInfo, nullptr, &m_commandPool ) );

	m_commandBuffers = std::vector<VkCommandBuffer>( RenderUtils::MAX_FRAMES_IN_FLIGHT );
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = m_commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = (uint32_t)m_commandBuffers.size();
	RETURN_LOG_ERROR( vkAllocateCommandBuffers( m_device->GetLogicalDevice(), &allocInfo, m_commandBuffers.data() ), "Failed to allocate command buffers" );
	return VK_SUCCESS;
}

VkResult Renderer::CreateDescriptorPool()
{
	VkDescriptorPoolSize descriptorTypeCounts[1]{};
	descriptorTypeCounts[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorTypeCounts[0].descriptorCount = RenderUtils::MAX_FRAMES_IN_FLIGHT;

	// Create the global descriptor pool
	// All descriptors used in this example are allocated from this pool
	VkDescriptorPoolCreateInfo descriptorPoolCI{};
	descriptorPoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCI.pNext = nullptr;
	descriptorPoolCI.poolSizeCount = 1;
	descriptorPoolCI.pPoolSizes = descriptorTypeCounts;

	// Set the max. number of descriptor sets that can be requested from this pool (requesting beyond this limit will result in an error)
	descriptorPoolCI.maxSets = RenderUtils::MAX_FRAMES_IN_FLIGHT;
	CR_RETURN( vkCreateDescriptorPool( m_device->GetLogicalDevice(), &descriptorPoolCI, nullptr, &m_descriptorPool ) );
	return VK_SUCCESS;
}

VkResult Renderer::CreateSyncObjects()
{
	size_t images = m_swapchain->GetImageCount();
	m_imageAvailableSemaphores.resize( images );
	m_renderFinishedSemaphores.resize( images );
	m_inFlightFences.resize( images );
	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	for( size_t i = 0; i < images; i++ )
	{
		RETURN_LOG_ERROR( vkCreateSemaphore( m_device->GetLogicalDevice(), &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i] ), "Failed to create semaphores for a frame" );
		RETURN_LOG_ERROR( vkCreateSemaphore( m_device->GetLogicalDevice(), &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i] ), "Failed to create semaphores for a frame" );
		RETURN_LOG_ERROR( vkCreateFence( m_device->GetLogicalDevice(), &fenceInfo, nullptr, &m_inFlightFences[i] ), "Failed to create fence for a frame" );
	}
	return VK_SUCCESS;
}

Device* Renderer::GetDevice() const
{
	return m_device;
}

VkAllocationCallbacks* Renderer::GetAllocator() const
{
	return m_allocator;
}

VkInstance Renderer::GetVulkanInstance() const
{
	return m_instance;
}

VkCommandBuffer Renderer::GetCurrentCommandBuffer() const
{
	return m_commandBuffers[m_currentFrame];
}

VkCommandPool Renderer::GetCommandPool() const
{
	return m_commandPool;
}

VkRenderPass Renderer::GetRenderPass() const
{
	return m_renderPass;
}

uint32_t Renderer::GetCurrentFrame() const
{
	return m_currentFrame;
}

VkDescriptorPool Renderer::GetDescriptorPool() const
{
	return m_descriptorPool;
}

uint32_t Renderer::GetWidth() const
{
	return m_width;
}

uint32_t Renderer::GetHeight() const
{
	return m_height;
}

VkSurfaceKHR* Renderer::GetSurface()
{
	return &m_surface;
}
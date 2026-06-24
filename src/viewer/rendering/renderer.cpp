// Copyright © 2025 CCP ehf.

#include "renderer.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>

#include "renderingConsts.h"
#include "vulkan/vulkanerrors.h"


namespace
{
VKAPI_ATTR VkBool32 VKAPI_CALL validationCallback( VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData )
{
	switch( messageSeverity )
	{
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
		Log::Info( "[vulkan] validation layer (verbose): %s", pCallbackData->pMessage );
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
		Log::Info( "[vulkan] validation layer: %s", pCallbackData->pMessage );
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
		Log::Warning( "[vulkan] validation layer: %s", pCallbackData->pMessage );
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
		Log::Error( "[vulkan] validation layer: %s", pCallbackData->pMessage );
		break;
	default:
		Log::Info( "[vulkan] validation layer: %s", pCallbackData->pMessage );
		break;
	}

	return VK_FALSE;
}
}

Renderer::~Renderer()
{
	if( m_instance != VK_NULL_HANDLE )
	{
		VkDevice logicalDevice = m_device->GetLogicalDevice();

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

		vkDestroyCommandPool( logicalDevice, m_graphicsCommandPool, m_allocator );
		vkDestroyCommandPool( logicalDevice, m_computeCommandPool, m_allocator );
		for( auto& semaphore : m_renderFinishedSemaphores )
		{
			vkDestroySemaphore( logicalDevice, semaphore, m_allocator );
		}
		for( auto& semaphore : m_imageAvailableSemaphores )
		{
			vkDestroySemaphore( logicalDevice, semaphore, m_allocator );
		}
		for( auto& semaphore : m_computeFinishedSemaphores )
		{
			vkDestroySemaphore( logicalDevice, semaphore, m_allocator );
		}
		for( auto& semaphore : m_computeReadySemaphores )
		{
			vkDestroySemaphore( logicalDevice, semaphore, m_allocator );
		}
		for( uint32_t i = 0; i < RenderingConsts::MAX_FRAMES_IN_FLIGHT; ++i )
		{
			vkDestroyFence( logicalDevice, m_inFlightGraphicsFence[i], m_allocator );
			vkDestroyFence( logicalDevice, m_inFlightComputeFence[i], m_allocator );
		}
		vkDestroyDevice( logicalDevice, m_allocator );
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

	Log::Debug( "Instance Extensions:" );

	for( const auto extension : extensions )
	{
		Log::Debug( "\t%s", extension );
	}

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
	createInfo.flags = 0;
#ifdef APPLE
	createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR; // mac support
#endif
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
	debugCreateInfo.pfnUserCallback = validationCallback;
	debugCreateInfo.pNext = nullptr;
	createInfo.pNext = &debugCreateInfo;
	auto debugInstanceResult = vkCreateInstance( &createInfo, m_allocator, &m_instance );
	if( debugInstanceResult == VK_SUCCESS )
	{
		return VK_SUCCESS;
	}

	Log::Warning( "Could not create Vulkan instance with validation layer, VULKAN_SDK environment variable may be missing. Trying without validation layer." );
	// fallback to try to create an instance wihtout validation layer

#endif
	createInfo.enabledLayerCount = 0;
	createInfo.pNext = nullptr;

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
	m_depthTarget = Texture::Create( m_device, m_width, m_height, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT );

	ON_ERROR_LOG_AND_RETURN( CreateCommandPool(), "Could not create command pool" );
	ON_ERROR_LOG_AND_RETURN( CreateSyncObjects(), "Could not create sync objects" );
	ON_ERROR_LOG_AND_RETURN( CreateCommandBuffers(), "Could not create command buffers" );
	m_lastSemaphore = m_swapchain->GetImageCount() - 1;

	// Signal first used compute ready semaphore
	VkSubmitInfo computeSubmitInfo{};
	computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	computeSubmitInfo.signalSemaphoreCount = 1;
	computeSubmitInfo.pSignalSemaphores = &m_computeReadySemaphores[m_lastSemaphore];
	ON_ERROR_LOG_AND_RETURN( vkQueueSubmit( m_device->GetComputeQueue(), 1, &computeSubmitInfo, VK_NULL_HANDLE ), "Could not submit compute queue" );
	m_valid = true;
}

VkResult Renderer::CreateCommandBuffers()
{
	VkCommandBufferAllocateInfo graphicsCommandBufferAllocateInfo;
	graphicsCommandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
	graphicsCommandBufferAllocateInfo.commandPool = m_graphicsCommandPool;
	graphicsCommandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	graphicsCommandBufferAllocateInfo.pNext = nullptr;
	graphicsCommandBufferAllocateInfo.commandBufferCount = static_cast<uint32_t>( m_graphicsCommandBuffers.size() );
	CR_RETURN( vkAllocateCommandBuffers( m_device->GetLogicalDevice(), &graphicsCommandBufferAllocateInfo, m_graphicsCommandBuffers.data() ) );

	VkCommandBufferAllocateInfo computeComandBufferAllocateInfo;
	computeComandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
	computeComandBufferAllocateInfo.commandPool = m_computeCommandPool;
	computeComandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	computeComandBufferAllocateInfo.pNext = nullptr;
	computeComandBufferAllocateInfo.commandBufferCount = static_cast<uint32_t>( m_computeCommandBuffers.size() );
	CR_RETURN( vkAllocateCommandBuffers( m_device->GetLogicalDevice(), &computeComandBufferAllocateInfo, m_computeCommandBuffers.data() ) );

	return VK_SUCCESS;
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
	m_depthTarget = Texture::Create( m_device, m_width, m_height, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT );

	m_swapchain = new Swapchain();
	RETURN_ERROR( m_swapchain->Initialize( m_device, m_surface, m_allocator, m_width, m_height ) );

	return VK_SUCCESS;
}


VkResult Renderer::BeginRender()
{
	VkDevice device = m_device->GetLogicalDevice();

	vkWaitForFences( device, 1, &m_inFlightGraphicsFence[m_currentFrame], VK_TRUE, UINT64_MAX );
	CR( vkResetFences( device, 1, &m_inFlightGraphicsFence[m_currentFrame] ) );

	VkResult imageAquisitionResult = vkAcquireNextImageKHR( device,
															m_swapchain->GetVulkanSwapchain(),
															UINT64_MAX,
															m_imageAvailableSemaphores[m_currentSemaphore],
															VK_NULL_HANDLE,
															&m_imageIndex );


	VkCommandBuffer cmdBuffer = GetCurrentGraphicVkCommandBuffer();

	VkCommandBufferBeginInfo cmdBufInfo{};
	cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	CR_RETURN( vkBeginCommandBuffer( cmdBuffer, &cmdBufInfo ) );

	// With dynamic rendering there are no subpass dependencies, so we need to take care of proper layout transitions by using barriers
	CreateFrameFence();
	CreateDepthFence();

	if( imageAquisitionResult != VK_SUCCESS && imageAquisitionResult != VK_SUBOPTIMAL_KHR && imageAquisitionResult != VK_ERROR_OUT_OF_DATE_KHR )
	{
		throw std::runtime_error( "failed to acquire swap chain image!" );
	}

	return VK_SUCCESS;
}

VkResult Renderer::EndRender()
{
	auto cmdBuffer = GetCurrentGraphicVkCommandBuffer();
	// This set of barriers prepares the color image for presentation, we don't need to care for the depth image
	PresentFence();

	CR_RETURN( vkEndCommandBuffer( cmdBuffer ) );
	VkPipelineStageFlags waitDstStageMask[2] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT };
	VkSemaphore waitSemaphores[2] = { m_imageAvailableSemaphores[m_currentSemaphore], m_computeFinishedSemaphores[m_currentSemaphore] };
	VkSemaphore signalSemaphores[2] = { m_renderFinishedSemaphores[m_currentSemaphore], m_computeReadySemaphores[m_currentSemaphore] };

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 2;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitDstStageMask;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_graphicsCommandBuffers[m_currentFrame];
	submitInfo.signalSemaphoreCount = 2;
	submitInfo.pSignalSemaphores = signalSemaphores;
	CR_RETURN( vkQueueSubmit( m_device->GetGraphicsQueue(), 1, &submitInfo, m_inFlightGraphicsFence[m_currentFrame] ) );

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &m_renderFinishedSemaphores[m_currentSemaphore];

	VkSwapchainKHR swapChain = m_swapchain->GetVulkanSwapchain();
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapChain;

	presentInfo.pImageIndices = &m_imageIndex;

	auto result = vkQueuePresentKHR( m_device->GetPresentQueue(), &presentInfo );

	m_currentFrame = ( m_currentFrame + 1 ) % RenderingConsts::MAX_FRAMES_IN_FLIGHT;
	m_lastSemaphore = m_currentSemaphore;
	m_currentSemaphore = ( m_currentSemaphore + 1 ) % m_swapchain->GetImageCount();

	if( result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR && result != VK_ERROR_OUT_OF_DATE_KHR )
	{
		throw std::runtime_error( "failed to present swap chain image!" );
	}

	return VK_SUCCESS;
}

VkResult Renderer::BeginCompute()
{
	VkDevice device = m_device->GetLogicalDevice();

	vkWaitForFences( device, 1, &m_inFlightComputeFence[m_currentFrame], VK_TRUE, UINT64_MAX );
	CR( vkResetFences( device, 1, &m_inFlightComputeFence[m_currentFrame] ) );

	VkCommandBuffer cmdBuffer = GetCurrentComputeVkCommandBuffer();
	VkCommandBufferBeginInfo cmdBufInfo{};
	cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	CR_RETURN( vkBeginCommandBuffer( cmdBuffer, &cmdBufInfo ) );
	return VK_SUCCESS;
}

VkResult Renderer::EndCompute()
{
	auto cmdBuffer = GetCurrentComputeVkCommandBuffer();
	CR_RETURN( vkEndCommandBuffer( cmdBuffer ) );


	VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

	VkSubmitInfo computeSubmitInfo{};
	computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	computeSubmitInfo.commandBufferCount = 1;
	computeSubmitInfo.pCommandBuffers = &cmdBuffer;
	computeSubmitInfo.waitSemaphoreCount = 1;
	computeSubmitInfo.pWaitSemaphores = &m_computeReadySemaphores[m_lastSemaphore];
	computeSubmitInfo.pWaitDstStageMask = &waitDstStageMask;
	computeSubmitInfo.signalSemaphoreCount = 1;
	computeSubmitInfo.pSignalSemaphores = &m_computeFinishedSemaphores[m_currentSemaphore];

	CR_RETURN( vkQueueSubmit( m_device->GetComputeQueue(), 1, &computeSubmitInfo, m_inFlightComputeFence[m_currentFrame] ) );

	return VK_SUCCESS;
}

VkResult Renderer::CreateCommandPool()
{
	QueueFamilyIndices queueFamilyIndices = m_device->GetQueueFamilyIndices();
	VkCommandPoolCreateInfo graphicPoolInfo{};
	graphicPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	graphicPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	graphicPoolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
	CR_RETURN( vkCreateCommandPool( m_device->GetLogicalDevice(), &graphicPoolInfo, m_allocator, &m_graphicsCommandPool ) );

	VkCommandPoolCreateInfo computePoolInfo{};
	computePoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	computePoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	if( queueFamilyIndices.computeFamily.has_value() )
	{
		computePoolInfo.queueFamilyIndex = queueFamilyIndices.computeFamily.value();
	}
	else
	{
		computePoolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
	}
	CR_RETURN( vkCreateCommandPool( m_device->GetLogicalDevice(), &computePoolInfo, m_allocator, &m_computeCommandPool ) );

	return VK_SUCCESS;
}

VkResult Renderer::CreateDescriptorPool()
{
	VkDescriptorPoolSize descriptorTypeCounts[1]{};
	descriptorTypeCounts[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorTypeCounts[0].descriptorCount = RenderingConsts::MAX_FRAMES_IN_FLIGHT;

	// Create the global descriptor pool
	// All descriptors used in this example are allocated from this pool
	VkDescriptorPoolCreateInfo descriptorPoolCI{};
	descriptorPoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCI.pNext = nullptr;
	descriptorPoolCI.poolSizeCount = 1;
	descriptorPoolCI.pPoolSizes = descriptorTypeCounts;

	// Set the max. number of descriptor sets that can be requested from this pool (requesting beyond this limit will result in an error)
	descriptorPoolCI.maxSets = RenderingConsts::MAX_FRAMES_IN_FLIGHT;
	CR_RETURN( vkCreateDescriptorPool( m_device->GetLogicalDevice(), &descriptorPoolCI, m_allocator, &m_descriptorPool ) );
	return VK_SUCCESS;
}

VkResult Renderer::CreateSyncObjects()
{
	size_t images = m_swapchain->GetImageCount();
	m_imageAvailableSemaphores.resize( images );
	m_renderFinishedSemaphores.resize( images );
	m_computeFinishedSemaphores.resize( images );
	m_computeReadySemaphores.resize( images );
	m_inFlightGraphicsFence.resize( images );
	m_inFlightComputeFence.resize( images );
	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	for( size_t i = 0; i < images; i++ )
	{
		RETURN_LOG_ERROR( vkCreateSemaphore( m_device->GetLogicalDevice(), &semaphoreInfo, m_allocator, &m_imageAvailableSemaphores[i] ), "Failed to create semaphores for a frame" );
		RETURN_LOG_ERROR( vkCreateSemaphore( m_device->GetLogicalDevice(), &semaphoreInfo, m_allocator, &m_renderFinishedSemaphores[i] ), "Failed to create semaphores for a frame" );
		RETURN_LOG_ERROR( vkCreateSemaphore( m_device->GetLogicalDevice(), &semaphoreInfo, m_allocator, &m_computeFinishedSemaphores[i] ), "Failed to create semaphores for a frame" );
		RETURN_LOG_ERROR( vkCreateSemaphore( m_device->GetLogicalDevice(), &semaphoreInfo, m_allocator, &m_computeReadySemaphores[i] ), "Failed to create semaphores for a frame" );
		RETURN_LOG_ERROR( vkCreateFence( m_device->GetLogicalDevice(), &fenceInfo, m_allocator, &m_inFlightGraphicsFence[i] ), "Failed to create fence for rendering" );
		RETURN_LOG_ERROR( vkCreateFence( m_device->GetLogicalDevice(), &fenceInfo, m_allocator, &m_inFlightComputeFence[i] ), "Failed to create fence for compute" );
	}
	return VK_SUCCESS;
}

void Renderer::CreateFrameFence() const
{
	VkCommandBuffer commandBuffer = GetCurrentGraphicVkCommandBuffer();
	const Texture* swapchainFrameTexture = m_swapchain->GetFrameTexture( m_imageIndex );

	VkImageMemoryBarrier imageMemoryBarrier{};
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemoryBarrier.srcAccessMask = 0;
	imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	imageMemoryBarrier.image = swapchainFrameTexture->GetImage();
	imageMemoryBarrier.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		0,
		0,
		nullptr,
		0,
		nullptr,
		1,
		&imageMemoryBarrier );
}

void Renderer::PresentFence() const
{
	VkCommandBuffer commandBuffer = GetCurrentGraphicVkCommandBuffer();
	const Texture* swapchainFrameTexture = m_swapchain->GetFrameTexture( m_imageIndex );

	VkImageMemoryBarrier imageMemoryBarrier{};
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	imageMemoryBarrier.dstAccessMask = 0;
	imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	imageMemoryBarrier.image = swapchainFrameTexture->GetImage();
	imageMemoryBarrier.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		0,
		0,
		nullptr,
		0,
		nullptr,
		1,
		&imageMemoryBarrier );
}

void Renderer::CreateDepthFence() const
{
	VkCommandBuffer commandBuffer = GetCurrentGraphicVkCommandBuffer();

	VkImageMemoryBarrier imageMemoryBarrier{};
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemoryBarrier.srcAccessMask = 0;
	imageMemoryBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	imageMemoryBarrier.image = m_depthTarget->GetImage();
	imageMemoryBarrier.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1 };

	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		0,
		0,
		nullptr,
		0,
		nullptr,
		1,
		&imageMemoryBarrier );
}

VkResult Renderer::CreateCopyCommandBuffer( VkCommandBuffer* commandBuffer ) const
{
	auto logical = GetDevice()->GetLogicalDevice();
	VkCommandBufferAllocateInfo cmdBufAllocateInfo{};
	cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBufAllocateInfo.commandPool = GetCommandPool();
	cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdBufAllocateInfo.commandBufferCount = 1;
	RETURN_LOG_ERROR( vkAllocateCommandBuffers( logical, &cmdBufAllocateInfo, commandBuffer ), "Failed to allocate command buffer for model buffer creation" );

	VkCommandBufferBeginInfo cmdBufInfo{};
	cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	RETURN_LOG_ERROR( vkBeginCommandBuffer( *commandBuffer, &cmdBufInfo ), "Failed to begin command buffer for model buffer creation" );
	return VK_SUCCESS;
}

VkResult Renderer::EndCopyCommandBuffer( VkCommandBuffer commandBuffer ) const
{
	RETURN_LOG_ERROR( vkEndCommandBuffer( commandBuffer ), "Failed to end copy command" );
	auto logical = GetDevice()->GetLogicalDevice();

	// Submit the command buffer to the queue to finish the copy
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	// Create fence to ensure that the command buffer has finished executing
	VkFenceCreateInfo fenceCI{};
	fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCI.flags = 0;
	VkFence fence;
	RETURN_LOG_ERROR( vkCreateFence( logical, &fenceCI, nullptr, &fence ), "Failed to create fence for model buffer copy" );

	// Submit to the queue
	RETURN_LOG_ERROR( vkQueueSubmit( GetDevice()->GetGraphicsQueue(), 1, &submitInfo, fence ), "Failed to submit the copy queue" );
	// Wait for the fence to signal that command buffer has finished executing
	RETURN_LOG_ERROR( vkWaitForFences( logical, 1, &fence, VK_TRUE, 100000000000 ), "Failed to wait for fence" );

	vkDestroyFence( logical, fence, nullptr );
	vkFreeCommandBuffers( logical, GetCommandPool(), 1, &commandBuffer );
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

VkCommandPool Renderer::GetCommandPool() const
{
	return m_graphicsCommandPool;
}

VkCommandBuffer Renderer::GetCurrentGraphicVkCommandBuffer() const
{
	return m_graphicsCommandBuffers[m_currentFrame];
}


VkCommandBuffer Renderer::GetCurrentComputeVkCommandBuffer() const
{
	return m_computeCommandBuffers[m_currentFrame];
}

const Texture* Renderer::GetCurrentSwapchainFrameTexture() const
{
	return m_swapchain->GetFrameTexture( m_imageIndex );
}

const Texture* Renderer::GetDepthTexture() const
{
	return m_depthTarget;
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

const Swapchain* Renderer::GetSwapchain() const
{
	return m_swapchain;
}

bool Renderer::HasDedicatedComputeQueue() const
{
	const QueueFamilyIndices indices = m_device->GetQueueFamilyIndices();
	return indices.computeFamily.has_value() && indices.computeFamily != indices.graphicsFamily;
}

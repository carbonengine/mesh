#include "renderer.h"
#include "device.h"
#include <fstream>
#include <stdexcept>
#include "shadercache.h"

namespace RenderUtils {
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

Renderer::Renderer() : 
    m_instance( VK_NULL_HANDLE ),
	m_surface( VK_NULL_HANDLE ),
	m_swapchain( VK_NULL_HANDLE ),
	m_renderPass( VK_NULL_HANDLE ),
	m_device( nullptr ),
	m_allocator( VK_NULL_HANDLE ),
	m_descriptorPool( VK_NULL_HANDLE ),
	m_descriptorSetLayout( VK_NULL_HANDLE ),
	m_currentFrame( 0 ),
	m_currentSemaphore( 0 ),
	m_depthTarget( nullptr ),
	m_commandPool( VK_NULL_HANDLE ),
	m_model( nullptr ),
	m_rot( 0.0f ),
	m_pipeline( VK_NULL_HANDLE )

{
}

Renderer::~Renderer() 
{	
    if( m_instance != VK_NULL_HANDLE )
    {
		VkDevice logicalDevice = m_device->GetLogicalDevice();
        if( m_pipeline != VK_NULL_HANDLE )
        {
			vkDestroyPipeline( logicalDevice, m_pipeline, m_allocator );
        }
        if( m_renderPass != VK_NULL_HANDLE )
        {
			vkDestroyRenderPass( logicalDevice, m_renderPass, m_allocator );
        }
        if( m_descriptorSetLayout != VK_NULL_HANDLE )
        {
			vkDestroyDescriptorSetLayout( logicalDevice, m_descriptorSetLayout, m_allocator );
        }
        if( m_descriptorPool != VK_NULL_HANDLE )
        {
			vkDestroyDescriptorPool( logicalDevice, m_descriptorPool, m_allocator );
        }

        m_shaderCache.Release( logicalDevice, m_allocator );

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
		if( m_model )
		{
			m_model->Release( m_device, m_allocator );
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
			vkDestroyBuffer( logicalDevice, m_perFrameBuffers[i].buffer, m_allocator );
			vkFreeMemory( logicalDevice, m_perFrameBuffers[i].memory, m_allocator );
		}
		vkDestroyInstance( m_instance, m_allocator );
	}
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

VkResult Renderer::init( uint32_t width, uint32_t height, VkSurfaceKHR surface )
{
	m_width = width;
	m_height = height;
	m_surface = surface;

	// Initialize device
	m_device = new Device();
	RETURN_ERROR( m_device->init( m_instance, m_allocator, m_surface ) );
	m_shaderCache = ShaderCache();
	RETURN_ERROR( m_shaderCache.Initialize( m_device->GetLogicalDevice() ) );
	m_swapchain = new Swapchain();
	RETURN_ERROR( m_swapchain->Initialize( m_device, m_surface, m_allocator, m_width, m_height ) );
    m_depthTarget = Texture::Create( m_device, m_width, m_height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT );

	RETURN_ERROR( CreateRenderPass() );
    RETURN_ERROR( m_swapchain->CreateFrameBuffers(m_device, m_renderPass, m_depthTarget) );
    RETURN_ERROR( CreateCommandBuffers() );
	RETURN_ERROR( CreateSyncObjects() );
	RETURN_ERROR( CreatePerFrameData() );
	RETURN_ERROR( CreateDescriptorPool() );
	RETURN_ERROR( CreateDescriptorSetLayout() );
	RETURN_ERROR( CreateDescriptorSets() );

    m_camera.at = Vector3(0, 0, 0);
	m_camera.pos = Vector3(0, 100, 100);
	m_camera.up = Vector3(0, 1, 0);

	return VK_SUCCESS;
}

VkInstance Renderer::GetVulkanInstance() const
{
    return m_instance;
}

void Renderer::SetModel( Model* model )
{
	m_model = model;
	m_model->Initialize( m_device, m_commandPool );

    if( m_pipeline != VK_NULL_HANDLE )
    {
        vkDestroyPipeline( m_device->GetLogicalDevice(), m_pipeline, m_allocator );
        m_pipeline = VK_NULL_HANDLE;
	}

    auto result = m_shaderCache.CreatePipeline( m_device->GetLogicalDevice(), "test", VK_POLYGON_MODE_FILL, m_renderPass, m_model->GetStride(), m_model->GetVertexDescriptions(), &m_pipeline );

    if( result != VK_SUCCESS )
    {
        CCP_LOGERR( "Failed to create pipeline for model" );
		m_model = nullptr;
		return;
	}

	m_camera.translation = IdentityMatrix();
	m_camera.at *= 0.0f;
	m_camera.pos *= 0.0f;
	m_camera.up = Vector3( 0, 1, 0 );
	Update(0.0f);
}


void Renderer::Update( float dt )
{
    // update camera and animations
	if(m_model)
	{
	    auto bs = m_model->GetBoundingSphere();
		m_camera.at = Vector3( bs.center );
		m_rot += dt * 1000.0f;
		Matrix translation = TranslationMatrix( bs.center + Vector3( 1.0, 0.25, 0.0) * bs.radius * 1.5f );
		Quaternion q = RotationQuaternion( Vector3( 0.0f, 1.0f, 0.0f ), m_rot );
		m_camera.translation = translation * RotationMatrix( q );// * rot;
		m_camera.pos = m_camera.translation.GetTranslation(); 
    }
}

VkResult Renderer::RenderFrame()
{
	VkDevice device = m_device->GetLogicalDevice();

	vkWaitForFences( device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX );
	CR( vkResetFences( device, 1, &m_inFlightFences[m_currentFrame] ) );

	uint32_t imageIndex{0};
	VkResult result = vkAcquireNextImageKHR( device, 
                                            m_swapchain->GetVulkanSwapchain(), 
                                            UINT64_MAX, 
                                            m_imageAvailableSemaphores[m_currentSemaphore], 
                                            VK_NULL_HANDLE, 
                                            &imageIndex );

	if( result == VK_ERROR_OUT_OF_DATE_KHR )
	{
		//throw std::runtime_error( "Swap chain is out of date!" );
		//recreateSwapChain();
		return result;
	}
	else if( result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR )
	{
		throw std::runtime_error( "failed to acquire swap chain image!" );
	}

    // update per frame data
	PerFrameData perframe{};
	perframe.proj = PerspectiveFovMatrix(1.0f, (float)m_width/(float)m_height, 0.1f, 30000.0f);
	perframe.view = LookAtMatrix(m_camera.pos, m_camera.at, m_camera.up);

	// Copy the current matrices to the current frame's uniform buffer
	// Note: Since we requested a host coherent memory type for the uniform buffer, the write is instantly visible to the GPU
	memcpy( m_perFrameBuffers[m_currentFrame].mapped, &perframe, sizeof( PerFrameData ) );

	auto res = RecordCommandBuffer( m_commandBuffers[m_currentFrame], imageIndex );
	if( res != VK_SUCCESS )
	{
		CCP_LOGERR( "[vulkan] Error: VkResult = %d", res );

		throw std::runtime_error( "failed to record command buffer!" );
    }

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

	presentInfo.pImageIndices = &imageIndex;

	result = vkQueuePresentKHR( m_device->GetPresentQueue(), &presentInfo );

	if( result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR )// || framebufferResized )
	{
		//framebufferResized = false;
		//recreateSwapChain();
		throw std::runtime_error( "failed to submit draw command buffer!" );

	}
	else if( result != VK_SUCCESS )
	{
		throw std::runtime_error( "failed to present swap chain image!" );
	}

	m_currentFrame = ( m_currentFrame + 1 ) % RenderUtils::MAX_FRAMES_IN_FLIGHT;
	m_currentSemaphore = ( m_currentSemaphore + 1 ) % m_swapchain->GetImageCount();
	return VK_SUCCESS;
}

VkResult Renderer::RecordCommandBuffer( VkCommandBuffer commandBuffer, uint32_t imageIndex )
{
	CR_RETURN( vkResetCommandBuffer( commandBuffer, /*VkCommandBufferResetFlagBits*/ 0 ) );

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	CR_RETURN( vkBeginCommandBuffer( commandBuffer, &beginInfo ) );
    
	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = m_renderPass;
	renderPassInfo.framebuffer = m_swapchain->GetFrameBuffer(imageIndex);
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
	viewport.y = (float) m_height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport( commandBuffer, 0, 1, &viewport );

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent.width = m_width;
	scissor.extent.height = m_height;

	vkCmdSetScissor( commandBuffer, 0, 1, &scissor );

    if( m_model )
	{
		vkCmdBindDescriptorSets( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shaderCache.GetPipelineLayout(), 0, 1, &m_perFrameBuffers[m_currentFrame].descriptorSet, 0, nullptr );
		vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline );
		m_model->Render( commandBuffer, 0, 0 );
    }

	vkCmdEndRenderPass( commandBuffer );

	CR_RETURN( vkEndCommandBuffer( commandBuffer ) );

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

	RETURN_LOG_ERROR( vkCreateRenderPass( m_device->GetLogicalDevice(), &renderPassInfo, nullptr, &m_renderPass ), "Failed to create render pass");
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

VkResult Renderer::CreatePerFrameData()
{
	// Create the uniform buffer
	VkDevice device = m_device->GetLogicalDevice();
    VkMemoryRequirements memReqs;

	// Vertex shader uniform buffer block
	VkBufferCreateInfo bufferInfo{};
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;
	allocInfo.allocationSize = 0;
	allocInfo.memoryTypeIndex = 0;

	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = sizeof( PerFrameData );
	// This buffer will be used as a uniform buffer
	bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

	// Create the buffers
	for( uint32_t i = 0; i < RenderUtils::MAX_FRAMES_IN_FLIGHT; i++ )
	{
		CR_RETURN( vkCreateBuffer( device, &bufferInfo, nullptr, &m_perFrameBuffers[i].buffer ) );
		// Get memory requirements including size, alignment and memory type
		vkGetBufferMemoryRequirements( device, m_perFrameBuffers[i].buffer, &memReqs );
		allocInfo.allocationSize = memReqs.size;
		// Get the memory type index that supports host visible memory access
		// Most implementations offer multiple memory types and selecting the correct one to allocate memory from is crucial
		// We also want the buffer to be host coherent so we don't have to flush (or sync after every update.
		// Note: This may affect performance so you might not want to do this in a real world application that updates buffers on a regular base
		allocInfo.memoryTypeIndex = m_device->GetMemoryTypeIndex( memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		// Allocate memory for the uniform buffer
		CR_RETURN( vkAllocateMemory( device, &allocInfo, nullptr, &( m_perFrameBuffers[i].memory ) ) );
		// Bind memory to buffer
		CR_RETURN( vkBindBufferMemory( device, m_perFrameBuffers[i].buffer, m_perFrameBuffers[i].memory, 0 ) );
		// We map the buffer once, so we can update it without having to map it again
		CR_RETURN( vkMapMemory( device, m_perFrameBuffers[i].memory, 0, sizeof( PerFrameData ), 0, (void**)&m_perFrameBuffers[i].mapped ) );
	}
    return VK_SUCCESS;
}

VkResult Renderer::CreateDescriptorSetLayout()
{
	// Binding 0: Uniform buffer (Vertex shader)
	VkDescriptorSetLayoutBinding layoutBinding{};
	layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	layoutBinding.descriptorCount = 1;
	layoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	layoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo descriptorLayoutCI{};
	descriptorLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorLayoutCI.pNext = nullptr;
	descriptorLayoutCI.bindingCount = 1;
	descriptorLayoutCI.pBindings = &layoutBinding;
	CR_RETURN( vkCreateDescriptorSetLayout( m_device->GetLogicalDevice(), &descriptorLayoutCI, nullptr, &m_descriptorSetLayout ) );
	m_shaderCache.CreatePipelineLayout( m_device->GetLogicalDevice(), m_descriptorSetLayout );
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

VkResult Renderer::CreateDescriptorSets()
{
	for( uint32_t i = 0; i < RenderUtils::MAX_FRAMES_IN_FLIGHT; i++ )
	{
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = m_descriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &m_descriptorSetLayout;
		CR_RETURN( vkAllocateDescriptorSets( m_device->GetLogicalDevice(), &allocInfo, &m_perFrameBuffers[i].descriptorSet ) );

		// Update the descriptor set determining the shader binding points
		// For every binding point used in a shader there needs to be one
		// descriptor set matching that binding point
		VkWriteDescriptorSet writeDescriptorSet{};

		// The buffer's information is passed using a descriptor info structure
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = m_perFrameBuffers[i].buffer;
		bufferInfo.range = sizeof( PerFrameData );

		// Binding 0 : Uniform buffer
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstSet = m_perFrameBuffers[i].descriptorSet;
		writeDescriptorSet.descriptorCount = 1;
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeDescriptorSet.pBufferInfo = &bufferInfo;
		writeDescriptorSet.dstBinding = 0;
		vkUpdateDescriptorSets( m_device->GetLogicalDevice(), 1, &writeDescriptorSet, 0, nullptr );
	}
	return VK_SUCCESS;
}
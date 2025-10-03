#include "swapchain.h"

Swapchain::Swapchain() :
	m_swapchain( VK_NULL_HANDLE ),
	m_swapchainExtent( { 0, 0 } ),
	m_swapchainImageFormat( VK_FORMAT_UNDEFINED )
{
}

Swapchain::~Swapchain()
{
}

VkResult Swapchain::init( Device* device, VkSurfaceKHR surface, const VkAllocationCallbacks* allocator, uint32_t width, uint32_t height )
{
	m_swapchainExtent = { width, height };

	RETURN_ERROR( CreateVulkanSwapchain( device, surface, allocator ) );
	return VK_SUCCESS;
}

VkResult Swapchain::CreateVulkanSwapchain( Device* device, VkSurfaceKHR surface, const VkAllocationCallbacks* allocator )
{
	SwapChainSupportDetails swapChainSupport = device->GetSwapchainSupportDetails();

	VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat( swapChainSupport.formats );
	VkPresentModeKHR presentMode = ChooseSwapPresentMode( swapChainSupport.presentModes );

	uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
	if( swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount )
	{
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = surface;

	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = m_swapchainExtent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	QueueFamilyIndices indices = device->GetQueueFamilyIndices();
	uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

	if( indices.graphicsFamily != indices.presentFamily )
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;

	VkDevice logicalDevice = device->GetLogicalDevice();

	RETURN_LOG_ERROR( vkCreateSwapchainKHR( logicalDevice, &createInfo, allocator, &m_swapchain ), "Failed to create swapchain" );

	std::vector<VkImage> swapChainImages;
	RETURN_ERROR( CR( vkGetSwapchainImagesKHR( logicalDevice, m_swapchain, &imageCount, nullptr ) ) );
	swapChainImages.resize( imageCount );
	RETURN_ERROR( CR( vkGetSwapchainImagesKHR( logicalDevice, m_swapchain, &imageCount, swapChainImages.data() ) ) );

	m_swapchainImageFormat = surfaceFormat.format;

	for( auto image : swapChainImages )
	{
		Texture* swapchainTexture = Texture::CreateFromImage( device, image, m_swapchainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT );
		if( !swapchainTexture )
		{
			return VK_ERROR_INITIALIZATION_FAILED;
		}
		m_swapchainFrames.push_back( swapchainTexture );
	}

	return VK_SUCCESS;
}

VkSurfaceFormatKHR Swapchain::ChooseSwapSurfaceFormat( const std::vector<VkSurfaceFormatKHR>& availableFormats )
{
	for( const auto& availableFormat : availableFormats )
	{
		if( availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR )
		{
			return availableFormat;
		}
	}

	return availableFormats[0];
}

VkPresentModeKHR Swapchain::ChooseSwapPresentMode( const std::vector<VkPresentModeKHR>& availablePresentModes )
{
	for( const auto& availablePresentMode : availablePresentModes )
	{
		if( availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR )
		{
			return availablePresentMode;
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

VkFormat Swapchain::GetFormat() const
{
	return m_swapchainImageFormat;
}

VkSwapchainKHR Swapchain::GetVulkanSwapchain() const
{
	return m_swapchain;
}

VkFramebuffer Swapchain::GetFrameBuffer( size_t index ) const
{
	return m_swapchainFramebuffers[index];
}

VkExtent2D Swapchain::GetExtent() const
{
	return m_swapchainExtent;
}

VkResult Swapchain::CreateFrameBuffers( Device* device, VkRenderPass renderPass, Texture* depth )
{
	m_swapchainFramebuffers.resize( m_swapchainFrames.size() );

	for( size_t i = 0; i < m_swapchainFramebuffers.size(); i++ )
	{
		std::array<VkImageView, 2> attachments = {
			m_swapchainFrames[i]->GetImageView(),
			depth->GetImageView()
		};

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = static_cast<uint32_t>( attachments.size() );
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = m_swapchainExtent.width;
		framebufferInfo.height = m_swapchainExtent.height;
		framebufferInfo.layers = 1;

		RETURN_ERROR( CR( vkCreateFramebuffer( device->GetLogicalDevice(), &framebufferInfo, nullptr, &m_swapchainFramebuffers[i] ) ) )
	}

	return VK_SUCCESS;
}


size_t Swapchain::GetImageCount() const
{
	return m_swapchainFrames.size();
}
#include "device.h"
#include <stdexcept>

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	VK_KHR_MAINTENANCE1_EXTENSION_NAME
};

const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

Device::Device() :
	m_logicalDevice( VK_NULL_HANDLE ),
	m_physicalDevice( VK_NULL_HANDLE ),
	m_graphicsQueue( VK_NULL_HANDLE ),
	m_presentQueue( VK_NULL_HANDLE ),
	m_familyIndices( {} ),
	m_swapchainSupport( {} )
{
}

Device::~Device()
{
	vkDestroyDevice( m_logicalDevice, nullptr );
}

VkResult Device::Initialize( VkInstance instance, const VkAllocationCallbacks* allocator, VkSurfaceKHR surface )
{
	RETURN_ERROR( pickPhysicalDevice( instance, allocator, surface ) );
	RETURN_ERROR( createLogicalDevice( allocator ) );
	return VK_SUCCESS;
}

VkResult Device::pickPhysicalDevice( VkInstance instance, const VkAllocationCallbacks* allocator, VkSurfaceKHR surface )
{
	uint32_t deviceCount = 0;
	CR_RETURN( vkEnumeratePhysicalDevices( instance, &deviceCount, nullptr ) );
	if( deviceCount == 0 )
	{
		CCP_LOGERR( "Failed to find GPUs with Vulkan support!" );
		return VK_ERROR_INITIALIZATION_FAILED;
	}
	std::vector<VkPhysicalDevice> devices( deviceCount );
	CR_RETURN( vkEnumeratePhysicalDevices( instance, &deviceCount, devices.data() ) );
	// Select the first device that meets the requirements
	for( const auto& device : devices )
	{
		if( IsDeviceSuitable( device, surface ) )
		{
			m_physicalDevice = device;

			break;
		}
	}
	if( m_physicalDevice == VK_NULL_HANDLE )
	{
		CCP_LOGERR( "Failed to find a suitable GPU!" );
		return VK_ERROR_INITIALIZATION_FAILED;
	}
	VkPhysicalDeviceProperties properties = {};
	vkGetPhysicalDeviceProperties( m_physicalDevice, &properties );
	vkGetPhysicalDeviceMemoryProperties( m_physicalDevice, &m_memoryProperties );
	CCP_LOGNOTICE( "Using GPU %s", properties.deviceName );
	return VK_SUCCESS;
}

VkResult Device::createLogicalDevice( const VkAllocationCallbacks* allocator )
{
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies = { m_familyIndices.graphicsFamily.value(), m_familyIndices.presentFamily.value() };

	float queuePriority = 1.0f;
	for( uint32_t queueFamily : uniqueQueueFamilies )
	{
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back( queueCreateInfo );
	}

	VkPhysicalDeviceFeatures deviceFeatures{};
	deviceFeatures.samplerAnisotropy = VK_TRUE;
	deviceFeatures.fillModeNonSolid = VK_TRUE;

	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

	createInfo.queueCreateInfoCount = static_cast<uint32_t>( queueCreateInfos.size() );
	createInfo.pQueueCreateInfos = queueCreateInfos.data();

	createInfo.pEnabledFeatures = &deviceFeatures;

	createInfo.enabledExtensionCount = static_cast<uint32_t>( deviceExtensions.size() );
	createInfo.ppEnabledExtensionNames = deviceExtensions.data();

#ifdef DEBUG_MODE
	createInfo.enabledLayerCount = static_cast<uint32_t>( validationLayers.size() );
	createInfo.ppEnabledLayerNames = validationLayers.data();
#else
	createInfo.enabledLayerCount = 0;
#endif

	RETURN_LOG_ERROR( vkCreateDevice( m_physicalDevice, &createInfo, allocator, &m_logicalDevice ), "Failed to create logical device" );

	vkGetDeviceQueue( m_logicalDevice, m_familyIndices.graphicsFamily.value(), 0, &m_graphicsQueue );
	vkGetDeviceQueue( m_logicalDevice, m_familyIndices.presentFamily.value(), 0, &m_presentQueue );

	return VK_SUCCESS;
}

bool Device::IsDeviceSuitable( VkPhysicalDevice device, VkSurfaceKHR surface )
{
	VkPhysicalDeviceProperties properties = {};
	vkGetPhysicalDeviceProperties( device, &properties );
	CCP_LOGNOTICE( "Checking GPU %s", properties.deviceName );
	QueueFamilyIndices indices = FindQueueFamilies( device, surface );

	bool extensionsSupported = DeviceSupportsExtensions( device );
	if( !extensionsSupported )
	{
		return false;
	}
	bool swapChainAdequate = false;
	SwapChainSupportDetails swapChainSupport;
	if( extensionsSupported )
	{
		swapChainSupport = QuerySwapChainSupport( device, surface );
		swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}

	VkPhysicalDeviceFeatures supportedFeatures;
	vkGetPhysicalDeviceFeatures( device, &supportedFeatures );

	if( indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy )
	{
		m_swapchainSupport = swapChainSupport;
		m_familyIndices = indices;
		return true;
	}

	return false;
}

QueueFamilyIndices Device::FindQueueFamilies( VkPhysicalDevice device, VkSurfaceKHR surface )
{
	QueueFamilyIndices indices;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties( device, &queueFamilyCount, nullptr );

	std::vector<VkQueueFamilyProperties> queueFamilies( queueFamilyCount );
	vkGetPhysicalDeviceQueueFamilyProperties( device, &queueFamilyCount, queueFamilies.data() );

	int i = 0;
	for( const auto& queueFamily : queueFamilies )
	{
		if( queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT )
		{
			indices.graphicsFamily = i;
		}

		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR( device, i, surface, &presentSupport );

		if( presentSupport )
		{
			indices.presentFamily = i;
		}

		if( indices.isComplete() )
		{
			break;
		}

		i++;
	}

	return indices;
}


SwapChainSupportDetails Device::QuerySwapChainSupport( VkPhysicalDevice device, VkSurfaceKHR surface )
{
	SwapChainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR( device, surface, &details.capabilities );

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR( device, surface, &formatCount, nullptr );

	if( formatCount != 0 )
	{
		details.formats.resize( formatCount );
		vkGetPhysicalDeviceSurfaceFormatsKHR( device, surface, &formatCount, details.formats.data() );
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR( device, surface, &presentModeCount, nullptr );

	if( presentModeCount != 0 )
	{
		details.presentModes.resize( presentModeCount );
		vkGetPhysicalDeviceSurfacePresentModesKHR( device, surface, &presentModeCount, details.presentModes.data() );
	}

	return details;
}

bool Device::DeviceSupportsExtensions( VkPhysicalDevice device )
{
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties( device, nullptr, &extensionCount, nullptr );

	std::vector<VkExtensionProperties> availableExtensions( extensionCount );
	vkEnumerateDeviceExtensionProperties( device, nullptr, &extensionCount, availableExtensions.data() );

	std::set<std::string> requiredExtensions( deviceExtensions.begin(), deviceExtensions.end() );

	for( const auto& extension : availableExtensions )
	{
		requiredExtensions.erase( extension.extensionName );
	}

	return requiredExtensions.empty();
}


SwapChainSupportDetails Device::GetSwapchainSupportDetails() const
{
	return m_swapchainSupport;
}

QueueFamilyIndices Device::GetQueueFamilyIndices() const
{
	return m_familyIndices;
}

VkDevice Device::GetLogicalDevice() const
{
	return m_logicalDevice;
}

VkPhysicalDevice Device::GetPhysicalDevice() const
{
	return m_physicalDevice;
}

VkQueue Device::GetGraphicsQueue() const
{
	return m_graphicsQueue;
}

VkQueue Device::GetPresentQueue() const
{
	return m_presentQueue;
}

VkPhysicalDeviceMemoryProperties Device::GetMemoryProperties() const
{
	return m_memoryProperties;
}

uint32_t Device::GetMemoryTypeIndex( uint32_t typeBits, VkMemoryPropertyFlags properties )
{
	for( uint32_t i = 0; i < m_memoryProperties.memoryTypeCount; i++ )
	{
		if( ( typeBits & 1 ) == 1 )
		{
			if( ( m_memoryProperties.memoryTypes[i].propertyFlags & properties ) == properties )
			{
				return i;
			}
		}
		typeBits >>= 1;
	}
	throw std::runtime_error( "Could not find a suitable memory type!" );
}
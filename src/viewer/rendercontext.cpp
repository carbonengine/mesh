#include "rendercontext.h"
#include <vector>
#include <algorithm>
#include <CCPLog.h>


#if !defined( NDEBUG )
	#define DEBUG_MODE

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugReport( VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData )
	{
		(void)flags;
		(void)object;
		(void)location;
		(void)messageCode;
		(void)pUserData;
		(void)pLayerPrefix; // Unused arguments
		CCP_LOGNOTICE("[vulkan] Debug report from ObjectType: %i", objectType);
		CCP_LOGNOTICE("\tMessage: %s", pMessage );
		return VK_FALSE;
	}

	static VKAPI_ATTR VkBool32 VKAPI_CALL validationCallback( VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData )
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

	static VkDebugReportCallbackEXT g_DebugReport = VK_NULL_HANDLE;

#endif

static VkResult ReportVulkanError( VkResult err, const char* file, int line, const char* caller )
{
	if( err != VK_SUCCESS )
	{
		CCP_LOGERR( "[vulkan] Error: VkResult = %d in %s:%d - for %s", err, file, line, caller );
	}
	return err;
}

#define CR( res ) ReportVulkanError( res, __FILE__, __LINE__, #res )

#define CR_RETURN( res ) \
	if( ReportVulkanError( res, __FILE__, __LINE__, #res ) != VK_SUCCESS ) \
	{															 \
		return res;											 \
	}   

#define RETURN_ERROR( res ) \
	if( res != VK_SUCCESS )									  \
	{															 \
		return res;											 \
	}

static std::vector<const char*> GetExtensions()
{
	// Enumerate available extensions
	uint32_t properties_count;
	std::vector<VkExtensionProperties> supportedExtensions;
	vkEnumerateInstanceExtensionProperties( nullptr, &properties_count, nullptr );
	supportedExtensions.resize( properties_count );
	CR( vkEnumerateInstanceExtensionProperties( nullptr, &properties_count, supportedExtensions.data() ) );

	std::vector<const char*> wantedExtensions{
		VK_KHR_SURFACE_EXTENSION_NAME,
		VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
	};

	// validate that we have all the wanted extensions
	std::vector<const char*> instanceExtensions;
	for( const auto& extension : supportedExtensions )
	{
		if( std::find( wantedExtensions.begin(), wantedExtensions.end(), extension.extensionName ) != wantedExtensions.end() )
		{
			instanceExtensions.push_back( extension.extensionName );
		}
	}

	#ifdef DEBUG_MODE
	instanceExtensions.push_back( VK_EXT_DEBUG_REPORT_EXTENSION_NAME );
	#endif

	return instanceExtensions;
}

RenderContext::RenderContext():
	m_allocator( nullptr ),
	m_instance( VK_NULL_HANDLE ),
	m_physicalDevice( VK_NULL_HANDLE ),
	m_device( VK_NULL_HANDLE ),
	m_queue( VK_NULL_HANDLE ),
	m_pipelineCache( VK_NULL_HANDLE ),
	m_descriptorPool( VK_NULL_HANDLE ),
	m_surface( VK_NULL_HANDLE )
{
}

RenderContext::~RenderContext()
{
}

VkResult RenderContext::Init( GLFWwindow* window )
{
	// intance creation
	RETURN_ERROR( CreateInstance() );
	CR( SetupDebugMessenger() );
	RETURN_ERROR( CreateSurface( window ) );

	return VkResult::VK_SUCCESS;
}

VkResult RenderContext::CreateInstance()
{
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

	auto extensions = GetExtensions();

	createInfo.enabledExtensionCount = (uint32_t)extensions.size();
	createInfo.ppEnabledExtensionNames = extensions.data();

#ifdef DEBUG_MODE
	const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
	createInfo.enabledLayerCount = 1;
	createInfo.ppEnabledLayerNames = layers;;
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
	debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debugCreateInfo.pfnUserCallback = validationCallback;
	createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
#endif

	RETURN_ERROR( CR( vkCreateInstance( &createInfo, m_allocator, &m_instance ) ) );

	return VkResult::VK_SUCCESS;
}

VkResult RenderContext::CreateSurface( GLFWwindow* window )
{
	if( window == nullptr )
	{
		CCP_LOGERR( "No window provided for Vulkan surface creation!" );
		return VK_ERROR_INITIALIZATION_FAILED;
	}
	return CR( glfwCreateWindowSurface( m_instance, window, m_allocator, &m_surface ) );
}

VkResult RenderContext::Render()
{
	return VkResult::VK_SUCCESS;
}

void RenderContext::Update(float dt)
{
}

VkResult RenderContext::Resize()
{
	return VkResult::VK_SUCCESS;
}

void RenderContext::Cleanup()
{
}

void RenderContext::BeginFrame()
{
}

void RenderContext::EndFrame()
{
}

void RenderContext::RenderMesh()
{
}   

void RenderContext::DrawUI()
{
}

VkResult RenderContext::SetupDebugMessenger()
{
#ifdef DEBUG_MODE
	const auto pvkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr( m_instance, "vkCreateDebugReportCallbackEXT" );
	if( pvkCreateDebugReportCallbackEXT == nullptr )
	{
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}

	VkDebugReportCallbackCreateInfoEXT debug_report_ci = {};
	debug_report_ci.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
	debug_report_ci.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
	debug_report_ci.pfnCallback = debugReport;
	debug_report_ci.pUserData = nullptr;
	return pvkCreateDebugReportCallbackEXT( m_instance, &debug_report_ci, m_allocator, &g_DebugReport );

#else
	return VK_SUCCESS;
#endif
}
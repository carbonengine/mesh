// Copyright © 2026 CCP ehf.

#include "vulkanerrors.h"

namespace VulkanResults
{

const char* VkResultAsString( VkResult res )
{
	switch( res )
	{
	case VK_SUCCESS:
		return "VK_SUCCESS";
	case VK_NOT_READY:
		return "VK_NOT_READY";
	case VK_TIMEOUT:
		return "VK_TIMEOUT";
	case VK_EVENT_SET:
		return "VK_EVENT_SET";
	case VK_EVENT_RESET:
		return "VK_EVENT_RESET";
	case VK_INCOMPLETE:
		return "VK_INCOMPLETE";
	case VK_ERROR_OUT_OF_HOST_MEMORY:
		return "VK_ERROR_OUT_OF_HOST_MEMORY";
	case VK_ERROR_OUT_OF_DEVICE_MEMORY:
		return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
	case VK_ERROR_INITIALIZATION_FAILED:
		return "VK_ERROR_INITIALIZATION_FAILED";
	case VK_ERROR_DEVICE_LOST:
		return "VK_ERROR_DEVICE_LOST";
	case VK_ERROR_MEMORY_MAP_FAILED:
		return "VK_ERROR_MEMORY_MAP_FAILED";
	case VK_ERROR_LAYER_NOT_PRESENT:
		return "VK_ERROR_LAYER_NOT_PRESENT";
	case VK_ERROR_EXTENSION_NOT_PRESENT:
		return "VK_ERROR_EXTENSION_NOT_PRESENT";
	case VK_ERROR_FEATURE_NOT_PRESENT:
		return "VK_ERROR_FEATURE_NOT_PRESENT";
	case VK_ERROR_INCOMPATIBLE_DRIVER:
		return "VK_ERROR_INCOMPATIBLE_DRIVER";
	case VK_ERROR_TOO_MANY_OBJECTS:
		return "VK_ERROR_TOO_MANY_OBJECTS";
	case VK_ERROR_FORMAT_NOT_SUPPORTED:
		return "VK_ERROR_FORMAT_NOT_SUPPORTED";
	case VK_ERROR_FRAGMENTED_POOL:
		return "VK_ERROR_FRAGMENTED_POOL";
	case VK_ERROR_UNKNOWN:
		return "VK_ERROR_UNKNOWN";
	case VK_ERROR_OUT_OF_POOL_MEMORY:
		return "VK_ERROR_OUT_OF_POOL_MEMORY";
	case VK_ERROR_INVALID_EXTERNAL_HANDLE:
		return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
	case VK_ERROR_FRAGMENTATION:
		return "Fragmentation";
	case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
		return "Invalid opaque capture address";
	case VK_PIPELINE_COMPILE_REQUIRED:
		return "Pipeline compile required";
	case VK_ERROR_SURFACE_LOST_KHR:
		return "Surface lost (KHR)";
	case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
		return "Native window in use (KHR)";
	case VK_SUBOPTIMAL_KHR:
		return "Suboptimal (KHR)";
	case VK_ERROR_OUT_OF_DATE_KHR:
		return "Out of date (KHR)";
	case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
		return "Incompatible display (KHR)";
	case VK_ERROR_INVALID_SHADER_NV:
		return "Invalid shader (NV)";
	case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR:
		return "Image usage not supported (KHR)";
	case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR:
		return "Video picture layout not supported (KHR)";
	case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR:
		return "Video profile operation not supported (KHR)";
	case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR:
		return "Video profile format not supported (KHR)";
	case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR:
		return "Video profile codec not supported (KHR)";
	case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR:
		return "Video std version not supported (KHR)";
	case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT:
		return "Invalid DRM format modifier plane layout (EXT)";
	case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
		return "Full screen exclusive mode lost (EXT)";
	case VK_THREAD_IDLE_KHR:
		return "Thread idle (KHR)";
	case VK_THREAD_DONE_KHR:
		return "Thread done (KHR)";
	case VK_OPERATION_DEFERRED_KHR:
		return "Operation deferred (KHR)";
	case VK_OPERATION_NOT_DEFERRED_KHR:
		return "Operation not deferred (KHR)";
	case VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR:
		return "Invalid video std parameters (KHR)";
	case VK_ERROR_COMPRESSION_EXHAUSTED_EXT:
		return "Compression exhausted (EXT)";
	case VK_INCOMPATIBLE_SHADER_BINARY_EXT:
		return "Incompatible shader binary (EXT)";
	case VK_PIPELINE_BINARY_MISSING_KHR:
		return "Pipeline binary missing (KHR)";
	case VK_ERROR_NOT_ENOUGH_SPACE_KHR:
		return "Not enough space (KHR)";
	case VK_ERROR_NOT_PERMITTED:
		return "Not permitted";
	default:
		return "Unrecognized VkResult";
	}
}

VkResult ReportVulkanError( VkResult err, const char* file, int line, const char* caller )
{
	if( err != VK_SUCCESS )
	{
		Log::Error( "[vulkan] %s in %s:%d - for %s\n", VkResultAsString( err ), file, line, caller );
	}
	return err;
}
}
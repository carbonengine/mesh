// Copyright © 2026 CCP ehf.

#pragma once

namespace VulkanResults
{
const char* VkResultAsString( VkResult res );
VkResult ReportVulkanError( VkResult err, const char* file, int line, const char* caller );
}

#define CR( res ) VulkanResults::ReportVulkanError( res, __FILE__, __LINE__, #res )

#define CR_RETURN( res )                                                                         \
	{                                                                                            \
		VkResult result = res;                                                                   \
		if( VulkanResults::ReportVulkanError( result, __FILE__, __LINE__, #res ) != VK_SUCCESS ) \
		{                                                                                        \
			return result;                                                                       \
		}                                                                                        \
	}

#define RETURN_ERROR( res )        \
	{                              \
		VkResult result = res;     \
		if( result != VK_SUCCESS ) \
		{                          \
			return result;         \
		}                          \
	}

#define RETURN_LOG_ERROR( res, message )                                             \
	{                                                                                \
		VkResult result = res;                                                       \
		if( result != VK_SUCCESS )                                                   \
		{                                                                            \
			Log::Error( "Vulkan result returned error(%d): %s\n", result, message ); \
			return result;                                                           \
		}                                                                            \
	}

#define ON_ERROR_LOG_AND_RETURN( res, message )                      \
	{                                                                \
		VkResult result = res;                                       \
		if( result != VK_SUCCESS )                                   \
		{                                                            \
			Log::Error( "Vulkan Error(%d): %s\n", result, message ); \
			return;                                                  \
		}                                                            \
	}

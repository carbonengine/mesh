#pragma once

#include <CCPLog.h>

#include <vulkan/vulkan.h>

#include <vector>
#include <array>
#include <set>
#include <algorithm>

#if !defined( NDEBUG )
#define DEBUG_MODE
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

#define CR_RETURN( res )                                                          \
	{                                                                             \
		VkResult result = res;                                                    \
		if( ReportVulkanError( result, __FILE__, __LINE__, #res ) != VK_SUCCESS ) \
		{                                                                         \
			return result;                                                        \
		}                                                                         \
	}

#define RETURN_ERROR( res )        \
	{                              \
		VkResult result = res;     \
		if( result != VK_SUCCESS ) \
		{                          \
			return result;         \
		}                          \
	}

#define RETURN_LOG_ERROR( res, message ) \
	{                                    \
		VkResult result = res;           \
		if( result != VK_SUCCESS )       \
		{                                \
			CCP_LOGERR( message );       \
                                         \
			return result;               \
		}                                \
	}

#define ON_ERROR_LOG_AND_RETURN( res, message ) \
    {                                          \
        VkResult result = res;                 \
        if( result != VK_SUCCESS )             \
        {                                      \
            CCP_LOGERR( message );             \
            return;                            \
        }                                      \
    }
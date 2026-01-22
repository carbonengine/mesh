#pragma once

#include <cmf/cmf.h>
//#include <vulkan/vulkan.h>

namespace VulkanEnums
{
VkFormat ElementTypeToVkFormat( cmf::ElementType element, uint8_t count );
}

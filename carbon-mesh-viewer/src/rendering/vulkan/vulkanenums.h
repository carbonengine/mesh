#pragma once
#include <vulkan/vulkan.h>
#include <cmf/cmf.h>

namespace VulkanEnums
{
VkFormat ElementTypeToVkFormat( cmf::ElementType element, uint8_t count );
}

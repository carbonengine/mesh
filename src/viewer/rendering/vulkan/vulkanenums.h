// Copyright © 2025 CCP ehf.

#pragma once

#include <cmf/cmf.h>

namespace VulkanEnums
{
VkFormat ElementTypeToVkFormat( cmf::ElementType element, uint8_t count );
}

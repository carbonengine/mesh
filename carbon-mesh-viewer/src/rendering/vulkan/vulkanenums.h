#pragma once

#include <cmf/cmf.h>

namespace VulkanEnums
{
VkFormat ElementTypeToVkFormat( cmf::ElementType element, uint8_t count );
void AsVertexInputAttributeDescriptions( const cmf::Span<cmf::VertexElement>& vertexDescriptions, std::vector<VkVertexInputAttributeDescription>& result );
}

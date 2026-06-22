// Copyright © 2025 CCP ehf.

#include "vulkanenums.h"

namespace VulkanEnums
{
VkFormat ElementTypeToVkFormat( cmf::ElementType element, uint8_t count )
{
	switch( element )
	{
	case cmf::ElementType::Float32:

		if( count == 4 )
		{
			return VK_FORMAT_R32G32B32A32_SFLOAT;
		}
		else if( count == 3 )
		{
			return VK_FORMAT_R32G32B32_SFLOAT;
		}
		else if( count == 2 )
		{
			return VK_FORMAT_R32G32_SFLOAT;
		}
		else if( count == 1 )
		{
			return VK_FORMAT_R32_SFLOAT;
		}
		break;
	case cmf::ElementType::Float16:
		if( count == 4 )
		{
			return VK_FORMAT_R16G16B16A16_SFLOAT;
		}
		else if( count == 3 )
		{
			return VK_FORMAT_R16G16B16_SFLOAT;
		}
		else if( count == 2 )
		{
			return VK_FORMAT_R16G16_SFLOAT;
		}
		else
		{
			return VK_FORMAT_R16_SFLOAT;
		}
		break;
	case cmf::ElementType::UInt16Norm:
		if( count == 4 )
		{
			return VK_FORMAT_R16G16B16A16_UNORM;
		}
		else if( count == 3 )
		{
			return VK_FORMAT_R16G16B16_UNORM;
		}
		else if( count == 2 )
		{
			return VK_FORMAT_R16G16_UNORM;
		}
		else if( count == 1 )
		{
			return VK_FORMAT_R16_UNORM;
		}
		break;
	case cmf::ElementType::UInt16:
		if( count == 4 )
		{
			return VK_FORMAT_R16G16B16A16_UINT;
		}
		else if( count == 3 )
		{
			return VK_FORMAT_R16G16B16_UINT;
		}
		else if( count == 2 )
		{
			return VK_FORMAT_R16G16_UINT;
		}
		else if( count == 1 )
		{
			return VK_FORMAT_R16_UINT;
		}
		break;
	case cmf::ElementType::Int16Norm:
		if( count == 4 )
		{
			return VK_FORMAT_R16G16B16A16_SNORM;
		}
		else if( count == 3 )
		{
			return VK_FORMAT_R16G16B16_SNORM;
		}
		else if( count == 2 )
		{
			return VK_FORMAT_R16G16_SNORM;
		}
		else if( count == 1 )
		{
			return VK_FORMAT_R16_SNORM;
		}
		break;
	case cmf::ElementType::Int16:
		if( count == 4 )
		{
			return VK_FORMAT_R16G16B16A16_SINT;
		}
		else if( count == 3 )
		{
			return VK_FORMAT_R16G16B16_SINT;
		}
		else if( count == 2 )
		{
			return VK_FORMAT_R16G16_SINT;
		}
		else if( count == 1 )
		{
			return VK_FORMAT_R16_SINT;
		}
		break;
	case cmf::ElementType::UInt8Norm:
		if( count == 4 )
		{
			return VK_FORMAT_R8G8B8A8_UNORM;
		}
		else if( count == 3 )
		{
			return VK_FORMAT_R8G8B8_UNORM;
		}
		else if( count == 2 )
		{
			return VK_FORMAT_R8G8_UNORM;
		}
		else if( count == 1 )
		{
			return VK_FORMAT_R8_UNORM;
		}
		break;

	case cmf::ElementType::UInt8:
		if( count == 4 )
		{
			return VK_FORMAT_R8G8B8A8_UINT;
		}
		else if( count == 3 )
		{
			return VK_FORMAT_R8G8B8_UINT;
		}
		else if( count == 2 )
		{
			return VK_FORMAT_R8G8_UINT;
		}
		else if( count == 1 )
		{
			return VK_FORMAT_R8_UINT;
		}
		break;
	case cmf::ElementType::Int8Norm:
		if( count == 4 )
		{
			return VK_FORMAT_R8G8B8A8_SNORM;
		}
		else if( count == 3 )
		{
			return VK_FORMAT_R8G8B8_SNORM;
		}
		else if( count == 2 )
		{
			return VK_FORMAT_R8G8_SNORM;
		}
		else if( count == 1 )
		{
			return VK_FORMAT_R8_SNORM;
		}
		break;

	case cmf::ElementType::Int8:
		if( count == 4 )
		{
			return VK_FORMAT_R8G8B8A8_SINT;
		}
		else if( count == 3 )
		{
			return VK_FORMAT_R8G8B8_SINT;
		}
		else if( count == 2 )
		{
			return VK_FORMAT_R8G8_SINT;
		}
		else if( count == 1 )
		{
			return VK_FORMAT_R8_SINT;
		}
		break;
	}
	return VK_FORMAT_UNDEFINED;
}

}

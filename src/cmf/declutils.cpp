// Copyright © 2026 CCP ehf.

#include "cmf/declutils.h"

namespace cmf
{

const VertexElement* FindElement( const Span<VertexElement>& decl, Usage usage, uint8_t usageIndex )
{
	for( auto& element : decl )
	{
		if( element.usage == usage && element.usageIndex == usageIndex )
		{
			return &element;
		}
	}
	return nullptr;
}

VertexElement* FindElement( Span<VertexElement>& decl, Usage usage, uint8_t usageIndex )
{
	for( auto& element : decl )
	{
		if( element.usage == usage && element.usageIndex == usageIndex )
		{
			return &element;
		}
	}
	return nullptr;
}

uint32_t GetElementTypeSize( ElementType type )
{
	switch( type )
	{
	case ElementType::Float32:
		return 4;

	case ElementType::Float16:
	case ElementType::UInt16Norm:
	case ElementType::UInt16:
	case ElementType::Int16Norm:
	case ElementType::Int16:
		return 2;

	case ElementType::UInt8Norm:
	case ElementType::UInt8:
	case ElementType::Int8Norm:
	case ElementType::Int8:
		return 1;

	default:
		return 0;
	}
}

uint32_t GetVertexElementSize( const VertexElement& element )
{
	return GetElementTypeSize( element.type ) * element.elementCount;
}

bool IsSignedElementType( ElementType type )
{
	switch( type )
	{
	case ElementType::Float32:
	case ElementType::Float16:
	case ElementType::Int16:
	case ElementType::Int16Norm:
	case ElementType::Int8:
	case ElementType::Int8Norm:
		return true;
	case ElementType::UInt16:
	case ElementType::UInt16Norm:
	case ElementType::UInt8:
	case ElementType::UInt8Norm:
		return false;
	}
	return false;
}

bool IsNormalizedElementType( ElementType type )
{
	switch( type )
	{
	case ElementType::Int16Norm:
	case ElementType::Int8Norm:
	case ElementType::UInt16Norm:
	case ElementType::UInt8Norm:
		return true;
	case ElementType::Float32:
	case ElementType::Float16:
	case ElementType::Int16:
	case ElementType::Int8:
	case ElementType::UInt16:
	case ElementType::UInt8:
		return false;
	}
	return false;
}

}
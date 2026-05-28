#pragma once

#include "cmf.h"

namespace cmf
{

CARBON_MESH_EXPORT const VertexElement* FindElement( const Span<VertexElement>& decl, Usage usage, uint8_t usageIndex = 0 );
CARBON_MESH_EXPORT VertexElement* FindElement( Span<VertexElement>& decl, Usage usage, uint8_t usageIndex = 0 );

CARBON_MESH_EXPORT uint32_t GetElementTypeSize( ElementType type );
CARBON_MESH_EXPORT uint32_t GetVertexElementSize( const VertexElement& element );
CARBON_MESH_EXPORT bool IsSignedElementType( ElementType type );

}
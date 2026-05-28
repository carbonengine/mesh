#pragma once

#include "cmf.h"

namespace cmf
{

/**
 * @brief Finds a vertex element in a vertex declaration by usage and index.
 * @param decl The vertex declaration to search.
 * @param usage The vertex usage type to find.
 * @param usageIndex The usage index to match. Defaults to 0.
 * @return A pointer to the matching vertex element, or nullptr if not found.
 */
CARBON_MESH_EXPORT const VertexElement* FindElement( const Span<VertexElement>& decl, Usage usage, uint8_t usageIndex = 0 );

/**
 * @brief Finds a vertex element in a vertex declaration by usage and index.
 * @param decl The vertex declaration to search.
 * @param usage The vertex usage type to find.
 * @param usageIndex The usage index to match. Defaults to 0.
 * @return A pointer to the matching vertex element, or nullptr if not found.
 */
CARBON_MESH_EXPORT VertexElement* FindElement( Span<VertexElement>& decl, Usage usage, uint8_t usageIndex = 0 );

/**
 * @brief Gets the storage size of the specified element type.
 * @param type The element type to query.
 * @return The size of the element type in bytes.
 */
CARBON_MESH_EXPORT uint32_t GetElementTypeSize( ElementType type );

/**
 * @brief Gets the total size of a vertex element based on its type and element count.
 * @param element The vertex element to query.
 * @return The total size of the vertex element in bytes, calculated as the size of the element type multiplied by the element count.
 */
CARBON_MESH_EXPORT uint32_t GetVertexElementSize( const VertexElement& element );
CARBON_MESH_EXPORT bool IsSignedElementType( ElementType type );

}
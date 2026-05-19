#pragma once

#include "cmf.h"
#include "memallocator.h"

namespace cmf
{

// Unapplies the index buffer to the vertex buffer, duplicating vertices as needed. Returns a new vertex buffer.
CARBON_MESH_EXPORT BufferView UnapplyIndexBuffer( const BufferView& vb, const BufferView& ib, MemoryAllocator& allocator, BufferManager& bufferManager );

// Makes a new identity index buffer (containing a sequence 0, 1, 2, 3, ... indexCount - 1).
CARBON_MESH_EXPORT BufferView MakeIdentityIndexBuffer( uint32_t indexCount, MemoryAllocator& allocator, BufferManager& bufferManager );

// Changes the vertex declaration of a buffer view, copying existing elements and zeroing new ones.
CARBON_MESH_EXPORT BufferView ChangeBufferVertexDeclaration( const BufferView& bufferView, const Span<VertexElement>& oldDecl, const Span<VertexElement>& newDecl, MemoryAllocator& allocator, BufferManager& bufferManager );

// Removes duplicate vertices from the given LOD vertex buffer and morph targets
CARBON_MESH_EXPORT void RemoveDuplicateVertices( MeshLod& lod, BufferManager& bufferManager );

// Converts an index buffer to 16-bit format if possible, returning the new index buffer. If the index buffer contains indices that are too large to fit in 16 bits, the original buffer view is returned.
CARBON_MESH_EXPORT BufferView ConvertTo16BitIndexBuffer( const BufferView& ib, MemoryAllocator& allocator, BufferManager& bufferManager );

/**
 * @brief Optimizes the buffers of a single LOD of a mesh. The function will reorder vertex and index buffers to improve vertex cache locality and reduce overdraw, which can improve rendering performance. 
 * The function will also remove duplicate vertices from the vertex buffer, which can further reduce the size of the buffers and improve performance. The function will reorder morph target buffers to match 
 * the new vertex order.
 * @param mesh The mesh to optimize buffers for.
 * @param lod The mesh level of detail whose buffers will be optimized. The LOD must be part of the provided mesh.
 * @param bufferManager The buffer manager used to handle the optimization.
 */
CARBON_MESH_EXPORT void OptimizeBuffers( const Mesh& mesh, MeshLod& lod, BufferManager& bufferManager );

/**
 * @brief Optimizes the buffers of all LODs of a mesh. The function will reorder vertex and index buffers to improve vertex cache locality and reduce overdraw, which can improve rendering performance. 
 * The function will also remove duplicate vertices from the vertex buffer, which can further reduce the size of the buffers and improve performance. The function will reorder morph target buffers to match 
 * the new vertex order.
 * @param mesh The mesh to optimize buffers for.
 * @param bufferManager The buffer manager used to handle the optimization.
 */
CARBON_MESH_EXPORT void OptimizeBuffers( Mesh& mesh, BufferManager& bufferManager );

}
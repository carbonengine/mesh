// Copyright © 2026 CCP ehf.

#pragma once

#include "cmf.h"
#include "memallocator.h"

namespace cmf
{

/**
 * @brief Converts indexed geometry to non-indexed geometry by expanding vertices according to the index buffer.
 * @param vb The original vertex buffer containing the indexed vertex data.
 * @param ib The original index buffer referencing vertices in the vertex buffer.
 * @param allocator The memory allocator used for allocating the expanded vertex buffer.
 * @param bufferManager The buffer manager used for managing the resulting buffer.
 * @return A buffer view containing the expanded non-indexed vertex data.
 */
CARBON_MESH_EXPORT BufferView UnapplyIndexBuffer( const BufferView& vb, const BufferView& ib, MemoryAllocator& allocator, BufferManager& bufferManager );

/**
 * @brief Creates an identity index buffer with sequential indices from 0 to indexCount-1.
 * @param indexCount The number of indices to generate in the buffer.
 * @param allocator The memory allocator used to allocate the buffer's memory.
 * @param bufferManager The buffer manager responsible for managing the created buffer.
 * @return A BufferView representing the created identity index buffer.
 */
CARBON_MESH_EXPORT BufferView MakeIdentityIndexBuffer( uint32_t indexCount, MemoryAllocator& allocator, BufferManager& bufferManager );

/**
 * @brief Changes the layout of the interleaved vertex buffer to match the new vertex declaration. Copies vertex data from the old buffer to a new 
   buffer according to the mapping between the old and new vertex declarations. Zeroes out any new vertex elements that do not have a corresponding
   element in the old declaration. The function does not modify the original buffer.
 * @param bufferView The buffer view whose vertex declaration will be changed.
 * @param oldDecl The old vertex declaration describing the current layout of the buffer.
 * @param newDecl The new vertex declaration describing the desired layout of the buffer.
 * @param allocator The memory allocator used to allocate memory for the new buffer.
 * @param bufferManager The buffer manager responsible for managing the new buffer.
 * @param alignment The alignment of the vertices. This uses newDecl's elements and may increase the stride of the resulting buffer view to conform to the alignment.
 * @return A BufferView representing the buffer with the layout matching the new vertex declaration.
 */
CARBON_MESH_EXPORT BufferView ChangeBufferVertexDeclaration( const BufferView& bufferView, const Span<VertexElement>& oldDecl, const Span<VertexElement>& newDecl, MemoryAllocator& allocator, BufferManager& bufferManager, uint32_t alignment = 1 );

/**
 * @brief Removes duplicate vertices from the given LOD vertex buffer and morph targets.
 * @param lod The mesh level of detail whose duplicate vertices will be removed.
 * @param bufferManager The buffer manager used to handle the operation.
 */
CARBON_MESH_EXPORT void RemoveDuplicateVertices( MeshLod& lod, BufferManager& bufferManager );

/**
 * @brief Converts an index buffer to 16-bit format. If the original index buffer is already 16-bit or if it contains indices that cannot be represented in 16 bits, 
   the original buffer will be returned without modification.
 * @param ib The source index buffer to convert.
 * @param allocator The memory allocator to use for the conversion.
 * @param bufferManager The buffer manager for handling buffer operations.
 * @return A BufferView representing the converted 16-bit index buffer.
 */
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
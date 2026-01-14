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

}
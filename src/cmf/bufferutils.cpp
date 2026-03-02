#include "cmf/bufferutils.h"
#include "cmf/bufferstreams.h"
#include "cmf/declutils.h"
#include <meshoptimizer.h>

namespace cmf
{

BufferView UnapplyIndexBuffer( const BufferView& vb, const BufferView& ib, MemoryAllocator& allocator, BufferManager& bufferManager )
{
	auto indexCount = ib.size / ib.stride;

	auto newVB = bufferManager.AllocateBuffer( nullptr, vb.stride * indexCount, vb.stride, SectionCompression::MeshOptimizerVertexBuffer );

	ConstIndexBufferStream oldIndices( ib, bufferManager );
	for( uint32_t i = 0; i < oldIndices.size(); ++i )
	{
		uint32_t index = oldIndices[i];
		memcpy(
			static_cast<uint8_t*>( bufferManager.GetData( newVB ) ) + i * vb.stride,
			static_cast<uint8_t*>( bufferManager.GetData( vb ) ) + index * vb.stride,
			vb.stride );
	}
	return newVB;
}

BufferView MakeIdentityIndexBuffer( uint32_t indexCount, MemoryAllocator& allocator, BufferManager& bufferManager )
{
	auto newIB = bufferManager.AllocateBuffer( nullptr, indexCount * sizeof( uint32_t ), sizeof( uint32_t ), SectionCompression::MeshOptimizerIndexBuffer );
	for( uint32_t i = 0; i < indexCount; ++i )
	{
		static_cast<uint32_t*>( bufferManager.GetData( newIB ) )[i] = i;
	}
	return newIB;
}

BufferView ChangeBufferVertexDeclaration( const BufferView& bufferView, const Span<VertexElement>& oldDecl, const Span<VertexElement>& newDecl, MemoryAllocator& allocator, BufferManager& bufferManager )
{
	auto vertexCount = bufferView.size / bufferView.stride;
	auto newVertexStride = 0u;
	for( const auto& element : newDecl )
	{
		newVertexStride += GetVertexElementSize( element );
	}
	auto newView = bufferManager.AllocateBuffer( nullptr, vertexCount * newVertexStride, newVertexStride, SectionCompression::MeshOptimizerVertexBuffer );

	struct Mapping
	{
		uint32_t srcOffset;
		uint32_t dstOffset;
		uint32_t size;
	};
	std::vector<Mapping> elementMapping;

	for( auto& newElement : newDecl )
	{
		auto oldElement = FindElement( oldDecl, newElement.usage, newElement.usageIndex );
		if( oldElement )
		{
			elementMapping.push_back( { oldElement->offset, newElement.offset, GetVertexElementSize( *oldElement ) } );
		}
		else
		{
			elementMapping.push_back( { 0xffffffff, newElement.offset, GetVertexElementSize( newElement ) } );
		}
	}

	auto oldBuffer = static_cast<const uint8_t*>( bufferManager.GetData( bufferView ) );
	auto newBuffer = static_cast<uint8_t*>( bufferManager.GetData( newView ) );

	for( uint32_t i = 0; i < vertexCount; ++i )
	{
		for( const auto& mapping : elementMapping )
		{
			if( mapping.srcOffset != 0xffffffff )
			{
				memcpy(
					newBuffer + i * newVertexStride + mapping.dstOffset,
					oldBuffer + i * bufferView.stride + mapping.srcOffset,
					mapping.size );
			}
			else
			{
				memset(
					newBuffer + i * newVertexStride + mapping.dstOffset,
					0,
					mapping.size );
			}
		}
	}
	return newView;
}

void RemoveDuplicateVertices( MeshLod& lod, BufferManager& bufferManager )
{
	auto indexData = bufferManager.GetData( lod.ib );
	uint32_t indexCount = lod.ib.size / lod.ib.stride;

	auto vertexData = bufferManager.GetData( lod.vb );
	uint32_t vertexCount = lod.vb.size / lod.vb.stride;
	uint32_t vertexStride = lod.vb.stride;

	std::vector<unsigned> remap( vertexCount );
	uint32_t newVertexCount;
	// TODO: we need to take morph targets into account when deduplicating vertices.
	if( lod.ib.stride == 4 )
	{
		newVertexCount = (uint32_t)meshopt_generateVertexRemap( remap.data(), reinterpret_cast<uint32_t*>( indexData ), indexCount, vertexData, vertexCount, vertexStride );
	}
	else
	{
		newVertexCount = (uint32_t)meshopt_generateVertexRemap( remap.data(), reinterpret_cast<uint16_t*>( indexData ), indexCount, vertexData, vertexCount, vertexStride );
	}
	if( newVertexCount == vertexCount )
	{
		// No duplicates found.
		return;
	}

	lod.vb = bufferManager.AllocateBuffer( nullptr, newVertexCount * vertexStride, vertexStride, cmf::SectionCompression::MeshOptimizerVertexBuffer );

	meshopt_remapVertexBuffer( bufferManager.GetData( lod.vb ), vertexData, vertexCount, vertexStride, remap.data() );
	if( lod.ib.stride == 4 )
	{
		meshopt_remapIndexBuffer( reinterpret_cast<uint32_t*>( indexData ), reinterpret_cast<uint32_t*>( indexData ), indexCount, remap.data() );
	}
	else
	{
		meshopt_remapIndexBuffer( reinterpret_cast<uint16_t*>( indexData ), reinterpret_cast<uint16_t*>( indexData ), indexCount, remap.data() );
	}
	for( auto& morph : lod.morphTargets )
	{
		auto morphData = bufferManager.GetData( morph.vb );
		morph.vb = bufferManager.AllocateBuffer( nullptr, newVertexCount * morph.vb.stride, morph.vb.stride, cmf::SectionCompression::MeshOptimizerVertexBuffer );
		meshopt_remapVertexBuffer( bufferManager.GetData( morph.vb ), morphData, vertexCount, morph.vb.stride, remap.data() );
	}
}

BufferView ConvertTo16BitIndexBuffer( const BufferView& ib, MemoryAllocator& allocator, BufferManager& bufferManager )
{
	if( ib.stride == 2 )
	{
		// Already 16-bit.
		return ib;
	}
	auto indexData = static_cast<const uint32_t*>( bufferManager.GetData( ib ) );
	uint32_t indexCount = ib.size / ib.stride;
	uint32_t maxIndex = 0;
	for( uint32_t i = 0; i < indexCount; ++i )
	{
		maxIndex = std::max( maxIndex, indexData[i] );
	}
	if( maxIndex > 0xffffu )
	{
		// Cannot convert to 16-bit because of large indices.
		return ib;
	}
	auto newIB = bufferManager.AllocateBuffer( nullptr, indexCount * sizeof( uint16_t ), sizeof( uint16_t ), SectionCompression::MeshOptimizerIndexBuffer );
	for( uint32_t i = 0; i < indexCount; ++i )
	{
		static_cast<uint16_t*>( bufferManager.GetData( newIB ) )[i] = (uint16_t)indexData[i];
	}
	return newIB;
}

}

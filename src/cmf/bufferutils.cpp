#include "cmf/bufferutils.h"
#include "cmf/bufferstreams.h"
#include "cmf/declutils.h"

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
		newVertexStride += static_cast<uint32_t>( GetVertexElementSize( element ) );
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
			elementMapping.push_back( { oldElement->offset, newElement.offset, static_cast<uint32_t>( GetVertexElementSize( *oldElement ) ) } );
		}
		else
		{
			elementMapping.push_back( { 0xffffffff, newElement.offset, static_cast<uint32_t>( GetVertexElementSize( *oldElement ) ) } );
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

}

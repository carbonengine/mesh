// Copyright © 2025 CCP ehf.

#include "cmf/memallocator.h"
#include "cmf/compression.h"

namespace cmf
{


String MemoryAllocator::AllocateString( std::string_view str )
{
	String result;
	Allocate( result, str.size() );
	memcpy( result.data(), str.data(), str.size() );
	return result;
}

void MemoryAllocator::Allocate( SpanRepr& a, size_t size )
{
	m_allocations.emplace_back( size );
	memcpy( m_allocations.back().data(), a.ptr, a.byteSize );
	a.ptr = m_allocations.back().data();
	a.byteSize = size;
}

void* MemoryAllocator::Allocate( size_t size )
{
	m_allocations.emplace_back( size );
	return m_allocations.back().data();
}


BufferManager::BufferManager( MemoryAllocator& allocator ) :
	m_allocator( allocator )
{
}

BufferView BufferManager::AllocateBuffer( const void* data, uint32_t size, uint32_t compressionStride )
{
	void* copy = m_allocator.Allocate( size );
	if( data )
	{
		memcpy( copy, data, size );
	}

	return AddBuffer( copy, size, compressionStride );
}

BufferView BufferManager::AddBuffer( void* data, uint32_t size, uint32_t stride )
{
	m_buffers.push_back( std::make_unique<Buffer>( Buffer{ data, size, {} } ) );
	return { uint32_t( m_buffers.size() - 1 ), 0, size, stride };
}

BufferView BufferManager::AddCompressedBuffer( void* data, uint32_t size, uint32_t stride, SectionCompression compression, uint32_t compressedSize )
{
	m_buffers.push_back( std::make_unique<Buffer>( Buffer{ data, size, compression, compressedSize, stride } ) );
	return { uint32_t( m_buffers.size() - 1 ), 0, size, stride };
}

void* BufferManager::GetData( const BufferView& view ) const
{
	const auto& buffer = m_buffers[view.index];
	if( buffer->compression != SectionCompression::None )
	{
		auto* decompressed = m_allocator.Allocate( buffer->size );
		const Section section = { 0, buffer->compressedSize, buffer->size, uint16_t( buffer->compressionStride ), SectionType::Data, buffer->compression };
		Decompress( decompressed, section, buffer->data );
		buffer->data = decompressed;
		buffer->compression = SectionCompression::None;
	}
	return static_cast<uint8_t*>( buffer->data ) + view.offset;
}

BufferManager::Buffer BufferManager::GetBuffer( uint32_t index ) const
{
	return *m_buffers[index];
}


}
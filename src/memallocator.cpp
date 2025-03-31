#include "cmf/memallocator.h"

namespace cmf
{


String MemoryAllocator::AllocateString( std::string_view str )
{
	String result;
	Allocate( result, str.size() );
	memcpy( result.ptr, str.data(), str.size() );
	return result;
}

void MemoryAllocator::Allocate( SpanRepr& a, size_t size )
{
	m_allocations.push_back( std::vector<uint8_t>( size ) );
	memcpy( m_allocations.back().data(), a.ptr, a.byteSize );
	a.ptr = m_allocations.back().data();
	a.byteSize = size;
}

BufferView BufferAllocator::AddBuffer( const void* data, uint32_t size, uint32_t stride )
{
	m_buffers.push_back( std::vector<uint8_t>{ reinterpret_cast<const uint8_t*>( data ), reinterpret_cast<const uint8_t*>( data ) + size } );
	return { uint32_t( m_buffers.size() - 1 ), size, stride };
}

void* BufferAllocator::GetBufferData( const BufferView& view )
{
	return m_buffers[view.offset].data();
}

const void* BufferAllocator::GetBufferData( const BufferView& view ) const
{
	return m_buffers[view.offset].data();
}

}
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

void* MemoryAllocator::Allocate( size_t size )
{
	m_allocations.push_back( std::vector<uint8_t>( size ) );
	return m_allocations.back().data();
}


BufferManager::BufferManager( MemoryAllocator& allocator )
    : m_allocator( allocator )
{
}

BufferView BufferManager::AllocateBuffer( const void* data, uint32_t size, uint32_t stride )
{
	m_buffers.push_back( { m_allocator.Allocate( size ), size } );
    memcpy( m_buffers.back().data, data, size );
	return { uint32_t( m_buffers.size() - 1 ), 0, size, stride };
}

BufferView BufferManager::AddBuffer( void* data, uint32_t size, uint32_t stride )
{
	m_buffers.push_back( { data, size } );
	return { uint32_t( m_buffers.size() - 1 ), 0, size, stride };
}

void BufferManager::SetBuffer( uint32_t index, void* data, uint32_t size )
{
	if( m_buffers.size() <= index )
    {
        m_buffers.resize( index + 1 );
	}
	m_buffers[index] = { data, size };
}

void* BufferManager::GetData( const BufferView& view ) const
{
	return static_cast<uint8_t*>( m_buffers[view.index].data ) + view.offset;
}

BufferManager::Buffer BufferManager::GetBuffer( uint32_t index ) const
{
	return m_buffers[index];
}


}
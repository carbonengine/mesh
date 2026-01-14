#pragma once

#include "cmf.h"
#include <vector>


namespace cmf
{

class MemoryAllocator
{
public:
	CARBON_MESH_EXPORT void Allocate( SpanRepr& a, size_t size );
	CARBON_MESH_EXPORT void* Allocate( size_t size );
	CARBON_MESH_EXPORT String AllocateString( std::string_view str );

    template <typename T>
	Span<T> AllocateSpan( size_t size )
	{
		SpanRepr a;
		Allocate( a, size * sizeof( T ) );
		return *reinterpret_cast<Span<T>*>( &a );
	}

private:
	std::vector<std::vector<uint8_t>> m_allocations;
};

class BufferManager
{
public:
    struct Buffer
    {
		void* data = nullptr;
		uint32_t size = 0;
		uint32_t compressionStride = 0;
		SectionCompression compression = SectionCompression::None;
    };

	CARBON_MESH_EXPORT explicit BufferManager( MemoryAllocator& allocator );

    CARBON_MESH_EXPORT BufferView AllocateBuffer( const void* data, uint32_t size, uint32_t compressionStride, SectionCompression compression );
	CARBON_MESH_EXPORT BufferView AddBuffer( void* data, uint32_t size, uint32_t compressionStride, SectionCompression compression );

	CARBON_MESH_EXPORT void SetBuffer( uint32_t index, void* data, uint32_t size );

    CARBON_MESH_EXPORT void* GetData( const BufferView& view ) const;
	CARBON_MESH_EXPORT Buffer GetBuffer( uint32_t index ) const;

private:
    MemoryAllocator& m_allocator;
	std::vector<Buffer> m_buffers;
};

template <typename T>
struct SpanModifier
{
	template <typename... Args>
	T& emplace_back( Args&&... args )
	{
		auto s = allocator.AllocateSpan<T>( data.size() + 1 );
		auto dest = static_cast<T*>( s.ptr );
		for( size_t i = 0; i < data.size(); ++i )
		{
			new( dest + i ) T{ std::move( data[i] ) };
		}
		new( dest + data.size() ) T{ std::forward<Args>( args )... };
		data = s;
		return data[data.size() - 1];
	}
	void push_back( const T& value )
	{
		emplace_back( value );
	}
	T& operator[]( size_t index )
	{
		return begin()[index];
	}
	size_t size() const
	{
		return data.size();
	}
	T* begin()
	{
		return data.begin();
	}
	T* end()
	{
		return data.end();
	}
    template <typename Iter>
    void insert( T* where, Iter srcBegin, Iter srcEnd )
    {
		if( where > end() || where < begin() || srcEnd < srcBegin )
		{
			return;
		}
		auto offset = size_t( where - begin() );
		auto length = size_t( srcEnd - srcBegin );
		auto s = allocator.AllocateSpan<T>( data.size() + length );
		auto dest = static_cast<T*>( s.ptr );
		for( size_t i = 0; i < offset; ++i )
		{
			new( dest + i ) T{ std::move( data[i] ) };
		}
		for( size_t i = 0; i < length; ++i )
		{
			new( dest + ( offset + i ) ) T{ *srcBegin };
			++srcBegin;
		}
		for( size_t i = offset; i < data.size(); ++i )
		{
			new( dest + ( i + length ) ) T{ std::move( data[i] ) };
		}
		data = s;
	}
	void insert( T* where, const T& value )
	{
		if( where > end() || where < begin() )
		{
			return;
		}
		auto s = allocator.AllocateSpan<T>( data.size() + 1 );
		auto dest = static_cast<T*>( s.ptr );
		size_t index = size_t( where - begin() );
		for( size_t i = 0; i < index; ++i )
		{
			new( dest + i ) T{ std::move( data[i] ) };
		}
		new( dest + index ) T{ value };
        for( size_t i = index; i < data.size(); ++i )
        {
            new( dest + ( i + 1 ) ) T{ std::move( data[i] ) };
		}
		data = s;
	}
	SpanModifier<T>& operator=( const std::vector<T>& other )
	{
		data = allocator.AllocateSpan<T>( other.size() );
		auto dest = static_cast<T*>( data.ptr );
		for( size_t i = 0; i < other.size(); ++i )
		{
			new( dest + i ) T{ other[i] };
		}
		return *this;
	}


	Span<T>& data;
	MemoryAllocator& allocator;
};

template <typename T>
SpanModifier<T> Modify( Span<T>& data, MemoryAllocator& allocator )
{
	return { data, allocator };
}

}
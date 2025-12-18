#pragma once

#include "cmf.h"
#include "cmf/memallocator.h"
#include <vector>
#include <memory>

namespace cmf
{


template <typename T>
constexpr bool ContainsSpans()
{
	if constexpr( std::is_base_of_v<SpanRepr, T> )
	{
		return true;
	}
	else
	{
		bool containsSpans = false;
		T t{};
		EnumerateMembers( t, [&containsSpans]( auto&&, auto&& value, const char* ) {
			containsSpans |= ContainsSpans<typename std::remove_reference<decltype( value )>::type>();
		} );
		return containsSpans;
	}
}

template <typename T>
size_t _GetSpanSizes( const T& value, size_t chunkAlignment )
{
	size_t size = 0;
	if constexpr( std::is_base_of_v<SpanRepr, T> )
	{
		size = chunkAlignment + value.byteSize;
		for( auto& element : value )
		{
			size += _GetSpanSizes( element, chunkAlignment );
		}
	}
	else
	{
		EnumerateMembers( const_cast<T&>( value ), [&size, chunkAlignment]( auto&&, auto& value, const char* ) {
			size += _GetSpanSizes( value, chunkAlignment );
		} );
	}
	return size;
}


template <typename T>
size_t GetFlattenedDataSizeEstimate( const T& value, size_t chunkAlignment )
{
	return sizeof( value ) + _GetSpanSizes( value, chunkAlignment );
}


struct FlattenedBuffer
{
	std::unique_ptr<uint8_t[]> data;
	size_t size;
};

struct FlattenVisitor
{
	template <typename P, typename T>
	void operator()( P&&, T& value, const char* )
	{
		EnumerateMembers( value, *this );
	}

	template <typename P, typename T>
	void operator()( P&&, Span<T>& value, const char* )
	{
		auto size = value.byteSize / sizeof( T );

		if constexpr( !ContainsSpans<T>() )
		{
			for( auto& chunk : m_chunks )
			{
				if( chunk.byteSize == value.byteSize && memcmp( chunk.ptr, value.ptr, value.byteSize ) == 0 )
				{
					value.ptr = chunk.ptr;
					return;
				}
			}
		}

		auto padding = ( m_buffer.size + m_chunkAlignment - 1 ) / m_chunkAlignment * m_chunkAlignment - m_buffer.size;
		std::fill_n( m_buffer.data.get() + m_buffer.size, padding, 0 );
		m_buffer.size += padding;

		memcpy( m_buffer.data.get() + m_buffer.size, value.ptr, value.byteSize );
		value.ptr = m_buffer.data.get() + m_buffer.size;
		m_buffer.size += value.byteSize;

		if constexpr( !ContainsSpans<T>() )
		{
			m_chunks.push_back( value );
		}

		for( auto& element : value )
		{
			if constexpr( std::is_same_v<T, String> )
			{
				operator()( value, element, "" );
			}
			else if constexpr( std::is_base_of_v<SpanRepr, typename std::remove_reference<T>::type> )
			{
				operator()( value, element, "" );
			}
			else
			{
				EnumerateMembers( element, *this );
			}
		}
	};

	FlattenedBuffer& m_buffer;
	size_t m_chunkAlignment = 4;
	std::vector<SpanRepr> m_chunks;
};

template <typename T>
FlattenedBuffer Flatten( const T& root, size_t chunkAlignment = 4 )
{
	size_t size = GetFlattenedDataSizeEstimate( root, chunkAlignment );
	FlattenedBuffer result{ std::make_unique<uint8_t[]>( size ), 0 };

	memcpy( result.data.get(), &root, sizeof( T ) );
	result.size = sizeof( T );

	EnumerateMembers( *reinterpret_cast<T*>( result.data.get() ), FlattenVisitor{ result, chunkAlignment } );
	return result;
}

std::vector<uint8_t> BuildFile( const Data& data, const BufferManager& buffers, const Metadata* metadata = nullptr );



}
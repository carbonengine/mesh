// Copyright © 2025 CCP ehf.

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
	size_t size = 0;
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
	size_t m_chunkAlignment = 8;
	std::vector<SpanRepr> m_chunks;
};

template <typename T>
FlattenedBuffer Flatten( const T& root, size_t chunkAlignment = 8 )
{
	size_t size = GetFlattenedDataSizeEstimate( root, chunkAlignment );
	FlattenedBuffer result{ std::make_unique<uint8_t[]>( size ), 0 };

	memcpy( result.data.get(), &root, sizeof( T ) );
	result.size = sizeof( T );

	EnumerateMembers( *reinterpret_cast<T*>( result.data.get() ), FlattenVisitor{ result, chunkAlignment } );
	return result;
}

/**
 * @brief Builds a file by serializing data and buffers into a byte vector. The resulting vector can be written to a file to create a valid CMF file. 
 * The function takes care of flattening the data structure, aligning buffer data, and including optional metadata if provided. Only buffers referenced 
 * by the data structure will be included in the output. The caller is responsible for ensuring that the data and buffers provided are valid and consistent with each other.
 * @param data The data to be serialized into the file.
 * @param buffers The buffer manager containing buffers to be included in the file.
 * @param metadata Optional metadata to include in the file. If null, no metadata is added.
 * @return A vector of bytes representing the built file contents.
 */
CARBON_MESH_EXPORT std::vector<uint8_t> BuildFile( const Data& data, const BufferManager& buffers, const Metadata* metadata = nullptr );



}
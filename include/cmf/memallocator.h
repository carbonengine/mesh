// Copyright © 2025 CCP ehf.

#pragma once

#include "cmf.h"
#include <vector>
#include <memory>


namespace cmf
{


/** @brief Class responsible for allocating memory for mesh data. It keeps track of all allocations and can be used to retrieve pointers to the allocated data. 
* All the allocated data stays alive as long as the MemoryAllocator instance is alive, and is automatically freed when the MemoryAllocator is destroyed.
*/
class MemoryAllocator
{
public:
	/** @brief Allocates a block of memory of the specified size and initialized the provided span with it. */
	CARBON_MESH_EXPORT void Allocate( SpanRepr& a, size_t size );
	/** @brief Allocates a block of memory of the specified size and returns a pointer to it. */
	CARBON_MESH_EXPORT void* Allocate( size_t size );
	/** @brief Allocates memory for a string and copies the provided string into it. The returned String will point to the allocated memory. */
	CARBON_MESH_EXPORT String AllocateString( std::string_view str );

	/** @brief Allocates memory for a Span with the specified number of elements. */
	template <typename T>
	Span<T> AllocateSpan( size_t size )
	{
		SpanRepr a;
		Allocate( a, size * sizeof( T ) );
		return *static_cast<Span<T>*>( &a );
	}

private:
	std::vector<std::vector<uint8_t>> m_allocations;
};

/** @brief Class responsible for managing buffers (sections). It keeps track of all allocated buffers and can be used to retrieve pointers to the buffer data. */
class BufferManager
{
public:
	struct Buffer
	{
		/** Pointer to the buffer data, may be compressed */
		void* data = nullptr;
		/** Size of the buffer in bytes (if the buffer is compressed, this is the decompressed size) */
		uint32_t size = 0;

		/** Compression used for the buffer (in case the buffer is compressed) */
		SectionCompression compression = SectionCompression::None;
		/** Size of the compressed buffer, if compression is not None. */
		uint32_t compressedSize = 0;
		/** Stride of the compressed buffer, if compression is not None. */
		uint32_t compressionStride = 0;
	};

	CARBON_MESH_EXPORT explicit BufferManager( MemoryAllocator& allocator );

	/** @brief Allocates a buffer with the specified data, size and stride. The data will be copied into the buffer unless it is null. 
    * @param data Pointer to the buffer data. If null, the buffer will be allocated but not initialized.
    * @param size Size of the buffer in bytes.
    * @param stride Stride in bytes between elements in the buffer, only used to initialize the returned BufferView.
    * @return A BufferView pointing to the beginning of the allocated buffer.
    */
	CARBON_MESH_EXPORT BufferView AllocateBuffer( const void* data, uint32_t size, uint32_t stride );

	/** @brief Adds a buffer with the specified data, size and stride. 
    * The class will not assume ownership of the data pointer, so it is the caller's responsibility to ensure that the data remains valid as long as the BufferManager is used
    * @param data Pointer to the buffer data. 
    * @param size Size of the buffer in bytes.
    * @param stride Stride in bytes between elements in the buffer, only used to initialize the returned BufferView.
    * @return A BufferView pointing to the beginning of the allocated buffer.
    */
	CARBON_MESH_EXPORT BufferView AddBuffer( void* data, uint32_t size, uint32_t stride );

	/** @brief Adds a compressed buffer with the specified data, size and stride.
    * The class will not assume ownership of the data pointer, so it is the caller's responsibility to ensure that the data remains valid as long as the BufferManager is used
    * @param data Pointer to the buffer data.
    * @param size Uncompressed size of the buffer in bytes.
    * @param stride Stride in bytes between elements in the buffer, only used to initialize the returned BufferView.
    * @param compression Compression used for the buffer.
    * @param compressedSize Size of the compressed buffer.
    * @return A BufferView pointing to the beginning of the allocated buffer.
    */
	CARBON_MESH_EXPORT BufferView AddCompressedBuffer( void* data, uint32_t size, uint32_t stride, SectionCompression compression, uint32_t compressedSize );

	/** @brief Retrieves a pointer to the data of the specified BufferView.
    * If the referenced buffer is compressed, it will be decompressed on the fly and the returned pointer will point to the decompressed data. 
    * The decompressed data will be stored internally and will remain valid as long as the BufferManager is alive.
    * @param view The BufferView for which to retrieve the data pointer.
    * @return A pointer to the data of the specified BufferView.
    */
	CARBON_MESH_EXPORT void* GetData( const BufferView& view ) const;

	/** @brief Retrieves the buffer data at the specified index.
    * @param index The index of the Buffer to retrieve.
    * @return The Buffer at the specified index.
    */
	[[nodiscard]] CARBON_MESH_EXPORT Buffer GetBuffer( uint32_t index ) const;

private:
	MemoryAllocator& m_allocator;
	std::vector<std::unique_ptr<Buffer>> m_buffers;
};

/**
 * @brief A helper class with std::vector-like interface to modify cmf::Span objects.
 * @tparam T The type of elements stored in the span.
 */
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
	void erase( T* where )
	{
		if( where >= end() || where < begin() )
		{
			return;
		}
		auto offset = size_t( where - begin() );
		auto s = allocator.AllocateSpan<T>( data.size() - 1 );
		auto dest = static_cast<T*>( s.ptr );
		for( size_t i = 0; i < offset; ++i )
		{
			new( dest + i ) T{ std::move( data[i] ) };
		}
		for( size_t i = offset + 1; i < data.size(); ++i )
		{
			new( dest + ( i - 1 ) ) T{ std::move( data[i] ) };
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

/**
 * @brief Creates a SpanModifier for the given span and memory allocator.
 * @tparam T The element type of the span.
 * @param data The span to be modified.
 * @param allocator The memory allocator to use for modifications.
 * @return A SpanModifier object that can modify the span using the provided allocator.
 */
template <typename T>
SpanModifier<T> Modify( Span<T>& data, MemoryAllocator& allocator )
{
	return { data, allocator };
}

}
// Copyright © 2026 CCP ehf.

#pragma once

#include "converters.h"
#include "memallocator.h"

namespace cmf
{

// NOLINTBEGIN(readability-identifier-naming)
/**
 * @brief A base class for data streams that provides access to elements of type T within a buffer.
 * @tparam T The destination type of elements in the stream when they are extracted from the buffer.
 * @tparam P The pointer type to the underlying data. Constness of P determines whether the stream allows modification of the data.
 * @tparam Converter The type converter used to convert between raw data and T.
 */
template <typename T, typename P, typename Converter = DeclTypeConverter<T>>
class BaseDataStream
{
public:
	using Byte = std::conditional_t<std::is_const_v<P>, const uint8_t, uint8_t>;

	BaseDataStream( Converter conversion, P* data, uint32_t count, uint32_t stride ) :
		m_conversion( conversion )
	{
		if( m_conversion )
		{
			m_data = static_cast<Byte*>( data );
			m_count = count;
			m_stride = stride;
		}
	}

	class Iterator
	{
	public:
		Iterator( Byte* data, uint32_t stride, Converter conversion ) :
			m_data( data ),
			m_stride( stride ),
			m_conversion( conversion )
		{
		}

		Iterator& operator++()
		{
			m_data += m_stride;
			return *this;
		}

		[[nodiscard]] bool operator==( const Iterator& other ) const
		{
			return m_data == other.m_data;
		}

		[[nodiscard]] bool operator!=( const Iterator& other ) const
		{
			return m_data != other.m_data;
		}

		// Const return type is intentional to prevent accidental use as an l-value like a "normal" iterator.
		[[nodiscard]] const T operator*() const // NOLINT( readability-const-return-type )
		{
			return m_conversion( m_data );
		}

		template <typename Q = P>
		std::enable_if_t<!std::is_const_v<Q>, void> set( const T& value ) const
		{
			m_conversion.set( m_data, value );
		}

	private:
		Byte* m_data;
		uint32_t m_stride;
		Converter m_conversion;
	};

	[[nodiscard]] Iterator begin() const
	{
		return Iterator( m_data, m_stride, m_conversion );
	}

	[[nodiscard]] Iterator end() const
	{
		return Iterator( m_data + m_stride * m_count, m_stride, m_conversion );
	};

	// Const return type is intentional to prevent accidental use as an l-value like a "normal" [] operator.
	[[nodiscard]] const T operator[]( uint32_t index ) const // NOLINT( readability-const-return-type )
	{
		return m_conversion( m_data + m_stride * index );
	}

	template <typename Q = P>
	std::enable_if_t<!std::is_const_v<Q>, void> set( uint32_t index, const T& value ) const
	{
		m_conversion.set( m_data + m_stride * index, value );
	}

	[[nodiscard]] uint32_t size() const
	{
		return m_count;
	}

	[[nodiscard]] bool exists() const
	{
		return m_data != nullptr;
	}

private:
	Byte* m_data = nullptr;
	uint32_t m_stride = 0;
	uint32_t m_count = 0;
	Converter m_conversion;
};

/**
 * @brief Calculates the number of elements in a buffer view based on its size and stride.
 * @param view The buffer view containing size and stride information.
 * @return The number of elements in the stream. Returns 0 if the view size is 0, returns 1 if stride is 0 (to avoid division by zero), otherwise returns size divided by stride.
 */
inline static uint32_t GetStreamElementCount( const BufferView& view )
{
	if( view.size == 0 )
	{
		return 0;
	}
	if( view.stride == 0 )
	{
		// Avoid division by zero; treat as a single element stream
		return 1;
	}
	return view.size / view.stride;
}

/**
 * @brief Base type for buffer element streams that provides access to vertex data stored in an interleaved buffer. This class is designed to access a specific 
   vertex element within an interleaved buffer
 * @tparam T The destination type of elements in the stream when they are extracted from the buffer. The stream will perform conversions from the raw buffer data to this type when accessing elements.
 * @tparam P The pointer type to the underlying data. Constness of P determines whether the stream allows modification of the data.
 */
template <typename T, typename P>
class BaseBufferElementStream : public BaseDataStream<T, P>
{
public:
	/**
	 * @brief Constructs a buffer element stream for accessing a specific vertex element within an interleaved buffer.
	 * @param element The vertex element describing the data layout, including type, count, and offset.
	 * @param data Pointer to the raw vertex buffer data. Note that the actual data pointer is adjusted by the element's offset to point to the correct location within the buffer.
	 * @param vertexCount The number of vertices in the buffer.
	 * @param stride The byte offset between consecutive vertices in the buffer.
	 */
	BaseBufferElementStream( const VertexElement& element, P* data, uint32_t vertexCount, uint32_t stride ) :
		BaseDataStream<T, P>( DeclTypeConverter<T>( element.type, element.elementCount ), static_cast<typename BaseDataStream<T, P>::Byte*>( data ) + element.offset, vertexCount, stride )
	{
		if( this->exists() )
		{
			m_element = element;
		}
	}

	/**
	 * @brief Constructs a buffer element stream for accessing a specific vertex element within an interleaved buffer. This constructor is a convenience overload that extracts 
	 * the necessary parameters from a BufferView structure, which describes the layout of the buffer data.
	 * @param element The vertex element describing the data format.
	 * @param buffer Pointer to the buffer data. The actual data pointer is adjusted by the view's offset to point to the correct location within the buffer, and the element's offset 
	 * is applied to access the specific vertex element.
	 * @param view The buffer view containing offset, count, and stride information.
	 */
	BaseBufferElementStream( const VertexElement& element, P* buffer, const BufferView& view ) :
		BaseBufferElementStream( element, static_cast<typename BaseDataStream<T, P>::Byte*>( buffer ) + view.offset, GetStreamElementCount( view ), view.stride )
	{
	}

	/**
	 * @brief Constructs a buffer element stream for accessing a specific vertex element within an interleaved buffer. This constructor is a convenience overload that retrieves the buffer 
	 * data from a BufferManager using a BufferView, which describes the layout of the buffer data.
	 * @param element The vertex element describing the data layout.
	 * @param view The buffer view specifying the data range and stride.
	 * @param buffers The buffer manager providing access to the underlying buffer data.
	 */
	BaseBufferElementStream( const VertexElement& element, const BufferView& view, const BufferManager& buffers ) :
		BaseBufferElementStream( element, buffers.GetData( view ), GetStreamElementCount( view ), view.stride )
	{
	}

	[[nodiscard]] const VertexElement& element() const
	{
		return m_element;
	}

private:
	VertexElement m_element = {};
};

/**
 * @brief A buffer stream is a container-like structure that provides an interface for accessing vertex data stored in an interleaved buffer. 
 * It abstracts away the details of how the data is laid out in memory, allowing users to access elements using standard container semantics (e.g., iterators, indexing).
 * The stream takes care of converting the raw byte data from the buffer into the appropriate types based on the vertex element description, 
 * making it easier to work with vertex attributes without needing to manually handle byte offsets and type conversions.
 * @tparam T Type of the vertex element data accessed through the stream (float, Vector3, etc.). The stream will perform conversions from the raw buffer data to this type when accessing elements.
 */
template <typename T>
using ConstBufferElementStream = BaseBufferElementStream<T, const void>;

/**
 * @brief A buffer stream is a container-like structure that provides an interface for accessing vertex data stored in an interleaved buffer. 
 * It abstracts away the details of how the data is laid out in memory, allowing users to access elements using standard container semantics (e.g., iterators, indexing).
 * The stream takes care of converting the raw byte data from the buffer into the appropriate types based on the vertex element description, 
 * making it easier to work with vertex attributes without needing to manually handle byte offsets and type conversions.
 * This class is similar to ConstBufferElementStream but allows for modification of the vertex data through the stream interface using method `set`.
 * @tparam T Type of the vertex element data accessed through the stream (float, Vector3, etc.). The stream will perform conversions to/from the raw buffer data to this type when accessing elements.
 */
template <typename T>
using BufferElementStream = BaseBufferElementStream<T, void>;


/**
 * @brief A templated class for accessing index buffer data, providing access to index data with automatic conversion between 16-bit and 32-bit index formats.
 * @tparam P The pointer type for the underlying buffer data. Constness of P determines whether the stream allows modification of the index data. 
 */
template <typename P>
class BaseIndexBufferStream : public BaseDataStream<uint32_t, P, IndexConverter>
{
	using Base = BaseDataStream<uint32_t, P, IndexConverter>;

public:
	/**
	 * @brief Constructs an index buffer stream with the specified buffer data, index count, and stride.
	 * @param data Pointer to the index buffer data.
	 * @param indexCount The number of indices in the buffer.
	 * @param stride The size in bytes between consecutive indices. Note that the only accepted stride values are 2 (for 16-bit indices) and 4 (for 32-bit indices).
	 */
	BaseIndexBufferStream( P* data, uint32_t indexCount, uint32_t stride ) :
		Base( IndexConverter( stride ), static_cast<typename Base::Byte*>( data ), indexCount, stride )
	{
	}

	/**
	 * @brief Constructs an index buffer stream from a data pointer and buffer view. The buffer view provides the necessary information about the offset, 
	 * size, and stride of the index data within the buffer. 
	 * @param data Pointer to the buffer data. The actual data pointer is adjusted by the view's offset to point to the correct location within the buffer.
	 * @param view The buffer view containing offset, size, and stride information.
	 */
	BaseIndexBufferStream( P* data, const BufferView& view ) :
		BaseIndexBufferStream( static_cast<typename Base::Byte*>( data ) + view.offset, GetStreamElementCount( view ), view.stride )
	{
	}

	/**
	 * @brief Constructs an index buffer stream from a buffer view and buffer manager. The buffer manager is used to retrieve the actual data pointer for
	 * the index buffer based on the information provided in the buffer view.
	 * @param view The buffer view containing offset, size, and stride information for the index data.
	 * @param buffers The buffer manager providing access to the underlying buffer data.
	 */
	BaseIndexBufferStream( const BufferView& view, const BufferManager& buffers ) :
		BaseIndexBufferStream( buffers.GetData( view ), GetStreamElementCount( view ), view.stride )
	{
	}
};
// NOLINTEND(readability-identifier-naming)

/**
 * @brief An index buffer stream is a container-like structure that provides an interface for accessing index data stored in an index buffer.
 * It simplifies accessing 16-bit or 32-bit index data by abstracting away the details of how the indices are laid out in memory and performing necessary conversions to a common type (uint32_t) for ease of use.
 */
using ConstIndexBufferStream = BaseIndexBufferStream<const void>;

/**
 * @brief An index buffer stream is a container-like structure that provides an interface for accessing index data stored in an index buffer.
 * It simplifies accessing 16-bit or 32-bit index data by abstracting away the details of how the indices are laid out in memory and performing necessary conversions to a common type (uint32_t) for ease of use.
 * This class is similar to ConstIndexBufferStream but allows for modification of the index data through the stream interface using method `set`.
 */
using IndexBufferStream = BaseIndexBufferStream<void>;

}
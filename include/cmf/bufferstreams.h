#pragma once

#include "converters.h"
#include "memallocator.h"

namespace cmf
{

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

		bool operator==( const Iterator& other ) const
		{
			return m_data == other.m_data;
		}

		bool operator!=( const Iterator& other ) const
		{
			return m_data != other.m_data;
		}

		T operator*() const
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

	Iterator begin() const
	{
		return Iterator( m_data, m_stride, m_conversion );
	}

	Iterator end() const
	{
		return Iterator( m_data + m_stride * m_count, m_stride, m_conversion );
	};

	T operator[]( uint32_t index ) const
	{
		return m_conversion( m_data + m_stride * index );
	}

	template <typename Q = P>
	std::enable_if_t<!std::is_const_v<Q>, void> set( uint32_t index, const T& value ) const
	{
		m_conversion.set( m_data + m_stride * index, value );
	}

	uint32_t size() const
	{
		return m_count;
	}

	bool exists() const
	{
		return m_data != nullptr;
	}

private:
	Byte* m_data = nullptr;
	uint32_t m_stride = 0;
	uint32_t m_count = 0;
	Converter m_conversion;
};


template <typename T, typename P>
class BaseBufferElementStream : public BaseDataStream<T, P>
{
public:
	BaseBufferElementStream( const VertexElement& element, P* data, uint32_t vertexCount, uint32_t stride ) :
		BaseDataStream<T, P>( DeclTypeConverter<T>( element.type, element.elementCount ), static_cast<typename BaseDataStream<T, P>::Byte*>( data ) + element.offset, vertexCount, stride )
	{
		if( this->exists() )
		{
			m_element = element;
		}
	}

	BaseBufferElementStream( const VertexElement& element, P* buffer, const BufferView& view ) :
		BaseBufferElementStream( element, static_cast<typename BaseDataStream<T, P>::Byte*>( buffer ) + view.offset, view.size / view.stride, view.stride )
	{
	}

	BaseBufferElementStream( const VertexElement& element, const BufferView& view, const BufferManager& buffers ) :
		BaseBufferElementStream( element, buffers.GetData( view ), view.size / view.stride, view.stride )
	{
	}

	const VertexElement& element() const
	{
		return m_element;
	}

private:
	VertexElement m_element = {};
};

template <typename T>
using ConstBufferElementStream = BaseBufferElementStream<T, const void>;
template <typename T>
using BufferElementStream = BaseBufferElementStream<T, void>;


template <typename P>
class BaseIndexBufferStream : public BaseDataStream<uint32_t, P, IndexConverter>
{
	using Base = BaseDataStream<uint32_t, P, IndexConverter>;

public:
	BaseIndexBufferStream( P* data, const BufferView& view ) :
		Base( IndexConverter( view.stride ), static_cast<typename Base::Byte*>( data ) + view.offset, view.size / view.stride, view.stride )
	{
	}

	BaseIndexBufferStream( const BufferView& view, const BufferManager& buffers ) :
		BaseIndexBufferStream( buffers.GetData( view ), view )
	{
	}
};

using ConstIndexBufferStream = BaseIndexBufferStream<const void>;
using IndexBufferStream = BaseIndexBufferStream<void>;

}
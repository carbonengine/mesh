#pragma once

#include "cmf.h"

namespace cmf
{



template <typename T>
using ConversionFunction = T ( * )( const void* );


template <typename T>
ConversionFunction<T> GetConversionFunction( ElementType type, uint8_t count )
{
	return []( const void* data ) { return *reinterpret_cast<const T*>( data ); };
}

template <>
ConversionFunction<float> GetConversionFunction( ElementType type, uint8_t count )
{
	switch( type )
	{
	case ElementType::Float32:
		return []( const void* data ) { return *reinterpret_cast<const float*>( data ); };
	case ElementType::Float16:
		return []( const void* data ) {
			return float( *reinterpret_cast<const Float_16*>( data ) );
		};
	case ElementType::UInt8Norm:
		return []( const void* data ) {
			return float( *reinterpret_cast<const uint8_t*>( data ) ) / 255.0f;
		};
	case ElementType::UInt8:
		return []( const void* data ) {
			return float( *reinterpret_cast<const uint8_t*>( data ) );
		};
	case ElementType::Int8Norm:
		return []( const void* data ) {
			return float( *reinterpret_cast<const int8_t*>( data ) ) / 127.0f;
		};
	case ElementType::Int8:
		return []( const void* data ) {
			return float( *reinterpret_cast<const int8_t*>( data ) );
		};
	default:
		return nullptr;
	}
}

template <>
ConversionFunction<Vector2> GetConversionFunction( ElementType type, uint8_t count )
{
	if( count < 2 )
	{
		return nullptr;
	}

	switch( type )
	{
	case ElementType::Float32:
		return []( const void* data ) { return *reinterpret_cast<const Vector2*>( data ); };
	case ElementType::Float16:
		return []( const void* data ) {
			return Vector2( *reinterpret_cast<const Vector2_16*>( data ) );
		};
	case ElementType::UInt8Norm:
		return []( const void* data ) {
			return Vector2(
				float( reinterpret_cast<const uint8_t*>( data )[0] ) / 255.0f,
				float( reinterpret_cast<const uint8_t*>( data )[1] ) / 255.0f );
		};
	case ElementType::UInt8:
		return []( const void* data ) {
			return Vector2(
				float( reinterpret_cast<const uint8_t*>( data )[0] ),
				float( reinterpret_cast<const uint8_t*>( data )[1] ) );
		};
	case ElementType::Int8Norm:
		return []( const void* data ) {
			return Vector2(
				float( reinterpret_cast<const int8_t*>( data )[0] ) / 127.0f,
				float( reinterpret_cast<const int8_t*>( data )[1] ) / 127.0f );
		};
	case ElementType::Int8:
		return []( const void* data ) {
			return Vector2(
				float( reinterpret_cast<const int8_t*>( data )[0] ),
				float( reinterpret_cast<const int8_t*>( data )[1] ) );
		};
	default:
		return nullptr;
	}
}

template <>
ConversionFunction<Vector3> GetConversionFunction( ElementType type, uint8_t count )
{
	if( count < 3 )
	{
		return nullptr;
	}

	switch( type )
	{
	case ElementType::Float32:
		return []( const void* data ) { return *reinterpret_cast<const Vector3*>( data ); };
	case ElementType::Float16:
		return []( const void* data ) {
			return Vector3( *reinterpret_cast<const Vector3_16*>( data ) );
		};
	case ElementType::UInt8Norm:
		return []( const void* data ) {
			return Vector3(
				float( reinterpret_cast<const uint8_t*>( data )[0] ) / 255.0f,
				float( reinterpret_cast<const uint8_t*>( data )[1] ) / 255.0f,
				float( reinterpret_cast<const uint8_t*>( data )[2] ) / 255.0f );
		};
	case ElementType::UInt8:
		return []( const void* data ) {
			return Vector3(
				float( reinterpret_cast<const uint8_t*>( data )[0] ),
				float( reinterpret_cast<const uint8_t*>( data )[1] ),
				float( reinterpret_cast<const uint8_t*>( data )[2] ) );
		};
	case ElementType::Int8Norm:
		return []( const void* data ) {
			return Vector3(
				float( reinterpret_cast<const int8_t*>( data )[0] ) / 127.0f,
				float( reinterpret_cast<const int8_t*>( data )[1] ) / 127.0f,
				float( reinterpret_cast<const int8_t*>( data )[2] ) / 127.0f );
		};
	case ElementType::Int8:
		return []( const void* data ) {
			return Vector3(
				float( reinterpret_cast<const int8_t*>( data )[0] ),
				float( reinterpret_cast<const int8_t*>( data )[1] ),
                float( reinterpret_cast<const int8_t*>( data )[2] ) );
		};
	default:
		return nullptr;
	}
}

template <>
ConversionFunction<Vector4> GetConversionFunction( ElementType type, uint8_t count )
{
	if( count != 4 )
	{
		return nullptr;
	}

	switch( type )
	{
	case ElementType::Float32:
		return []( const void* data ) { return *reinterpret_cast<const Vector4*>( data ); };
	case ElementType::Float16:
		return []( const void* data ) {
			return Vector4( *reinterpret_cast<const Vector4_16*>( data ) );
		};
	case ElementType::UInt8Norm:
		return []( const void* data ) {
			return Vector4(
				float( reinterpret_cast<const uint8_t*>( data )[0] ) / 255.0f,
				float( reinterpret_cast<const uint8_t*>( data )[1] ) / 255.0f,
				float( reinterpret_cast<const uint8_t*>( data )[2] ) / 255.0f,
				float( reinterpret_cast<const uint8_t*>( data )[3] ) / 255.0f );
		};
	case ElementType::UInt8:
		return []( const void* data ) {
			return Vector4(
				float( reinterpret_cast<const uint8_t*>( data )[0] ),
				float( reinterpret_cast<const uint8_t*>( data )[1] ),
				float( reinterpret_cast<const uint8_t*>( data )[2] ),
				float( reinterpret_cast<const uint8_t*>( data )[3] ) );
		};
	case ElementType::Int8Norm:
		return []( const void* data ) {
			return Vector4(
				float( reinterpret_cast<const int8_t*>( data )[0] ) / 127.0f,
				float( reinterpret_cast<const int8_t*>( data )[1] ) / 127.0f,
				float( reinterpret_cast<const int8_t*>( data )[2] ) / 127.0f,
				float( reinterpret_cast<const int8_t*>( data )[3] ) / 127.0f );
		};
	case ElementType::Int8:
		return []( const void* data ) {
			return Vector4(
				float( reinterpret_cast<const int8_t*>( data )[0] ),
				float( reinterpret_cast<const int8_t*>( data )[1] ),
				float( reinterpret_cast<const int8_t*>( data )[2] ),
				float( reinterpret_cast<const int8_t*>( data )[3] ) );
		};
	default:
		return nullptr;
	}
}


template <typename T>
class BufferElementStream
{
public:
	BufferElementStream( const VertexElement& element, const void* data, uint32_t vertexCount, uint32_t stride )
	{
		m_conversion = GetConversionFunction<T>( element.type, element.elementCount );
		m_element = element;
        if ( m_conversion )
        {
		    m_data = reinterpret_cast<const uint8_t*>( data ) + element.offset;
		    m_count = vertexCount;
		    m_stride = stride;
		}
	}

	class Iterator
	{
	public:
		Iterator( const uint8_t* data, uint32_t stride, ConversionFunction<T> conversion ) :
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

		bool operator!=( const Iterator& other ) const
		{
			return m_data != other.m_data;
		}

		T operator*() const
		{
			return m_conversion( m_data );
		}

	private:
		const uint8_t* m_data;
		uint32_t m_stride;
		ConversionFunction<T> m_conversion;
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

	uint32_t size() const
	{
		return m_count;
	}

	const VertexElement& element() const
	{
		return m_element;
	}

	bool exists() const
	{
		return m_data != nullptr;
	}

private:
	const uint8_t* m_data = nullptr;
	uint32_t m_stride = 0;
	uint32_t m_count = 0;
	ConversionFunction<T> m_conversion = {};
	VertexElement m_element = {};
};


bool IsHeaderValid( const void* data, size_t size );

CcpMath::AxisAlignedBox CalculateBounds( const Mesh& mesh, const void* vb );
CcpMath::AxisAlignedBox CalculateBounds( const Mesh& mesh, const void* vb, const void* ib, uint32_t firstElement, uint32_t elementCount );

}
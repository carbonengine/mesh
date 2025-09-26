#pragma once

#include "cmf.h"

namespace cmf
{


template <typename T>
using ConversionFunction = T ( * )( const void* );


template <typename From, typename To>
inline ConversionFunction<To> MakeConversionFunction()
{
    return []( const void* data ) { return To( *reinterpret_cast<const From*>( data ) ); };
}

template <typename From, typename To>
inline ConversionFunction<To> MakeNormalizedConversionFunction()
{
	return []( const void* data ) { return To( double( *reinterpret_cast<const From*>( data ) ) / double( std::numeric_limits<From>::max() ) ); };
}

template <typename T>
inline std::pair<ConversionFunction<T>, size_t> GetScalarConversionFunction( ElementType type )
{
	switch( type )
	{
	case ElementType::Float32:
		return { MakeConversionFunction<float, T>(), sizeof( float ) };
	case ElementType::Float16:
		return { MakeConversionFunction<Float_16, T>(), sizeof( Float_16 ) };
	case ElementType::UInt16Norm:
		return { MakeNormalizedConversionFunction<uint16_t, T>(), sizeof( uint16_t ) };
	case ElementType::UInt16:
		return { MakeConversionFunction<uint16_t, T>(), sizeof( uint16_t ) };
	case ElementType::Int16Norm:
		return { MakeNormalizedConversionFunction<int16_t, T>(), sizeof( int16_t ) };
	case ElementType::Int16:
		return { MakeConversionFunction<int16_t, T>(), sizeof( int16_t ) };
	case ElementType::UInt8Norm:
		return { MakeNormalizedConversionFunction<uint8_t, T>(), sizeof( uint8_t ) };
	case ElementType::UInt8:
		return { MakeConversionFunction<uint8_t, T>(), sizeof( uint8_t ) };
	case ElementType::Int8Norm:
		return { MakeNormalizedConversionFunction<int8_t, T>(), sizeof( int8_t ) };
	case ElementType::Int8:
		return { MakeConversionFunction<int8_t, T>(), sizeof( int8_t ) };
	default:
		return { nullptr, 0 };
	}
}

template <typename T>
struct DeclTypeConverter
{
    DeclTypeConverter( ElementType type, uint8_t count ) :
		m_func( GetScalarConversionFunction<T>( type ).first )
    {
    }

	T operator()( const void* data ) const
	{
		return m_func( data );
	}

    operator bool() const
    {
        return m_func != nullptr;
	}

    ConversionFunction<T> m_func = {};
};

template <>
struct DeclTypeConverter<Vector2>
{
	DeclTypeConverter( ElementType type, uint8_t count ) :
		m_func( GetScalarConversionFunction<float>( type ) ),
		m_count( std::min( count, uint8_t( 2 ) ) )
	{
	}

	Vector2 operator()( const void* data ) const
	{
		Vector2 result = { 0, 0 };
		const uint8_t* src = static_cast<const uint8_t*>( data );
        for ( uint8_t i = 0; i < m_count; ++i )
        {
			result[i] = m_func.first( src );
			src += m_func.second;
		}
		return result;
	}

	operator bool() const
	{
		return m_func.first != nullptr;
	}

	std::pair<ConversionFunction<float>, size_t> m_func = {};
	uint8_t m_count = 0;
};

template <>
struct DeclTypeConverter<Vector3>
{
	DeclTypeConverter( ElementType type, uint8_t count ) :
		m_func( GetScalarConversionFunction<float>( type ) ),
		m_count( std::min( count, uint8_t( 3 ) ) )
	{
	}

	Vector3 operator()( const void* data ) const
	{
		Vector3 result = { 0, 0, 0 };
		const uint8_t* src = static_cast<const uint8_t*>( data );
		for( uint8_t i = 0; i < m_count; ++i )
		{
			result[i] = m_func.first( src );
			src += m_func.second;
		}
		return result;
	}

	operator bool() const
	{
		return m_func.first != nullptr;
	}

	std::pair<ConversionFunction<float>, size_t> m_func = {};
	uint8_t m_count = 0;
};

template <>
struct DeclTypeConverter<Vector4>
{
	DeclTypeConverter( ElementType type, uint8_t count ) :
		m_func( GetScalarConversionFunction<float>( type ) ),
		m_count( std::min( count, uint8_t( 4 ) ) )
	{
	}

	Vector4 operator()( const void* data ) const
	{
		Vector4 result = { 0, 0, 0, 0 };
		const uint8_t* src = static_cast<const uint8_t*>( data );
		for( uint8_t i = 0; i < m_count; ++i )
		{
			result[i] = m_func.first( src );
			src += m_func.second;
		}
		return result;
	}

	operator bool() const
	{
		return m_func.first != nullptr;
	}

	std::pair<ConversionFunction<float>, size_t> m_func = {};
	uint8_t m_count = 0;
};


template <typename T>
class BufferElementStream
{
public:
	BufferElementStream( const VertexElement& element, const void* data, uint32_t vertexCount, uint32_t stride ) :
		m_conversion( element.type, element.elementCount )
	{
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
		Iterator( const uint8_t* data, uint32_t stride, DeclTypeConverter<T> conversion ) :
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
		DeclTypeConverter<T> m_conversion;
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
	DeclTypeConverter<T> m_conversion;
	VertexElement m_element = {};
};


bool IsHeaderValid( const void* data, size_t size );
uint32_t ComputeCrc32( const void* data, size_t size );


CcpMath::AxisAlignedBox CalculateBounds( const Mesh& mesh, const void* vb );
CcpMath::AxisAlignedBox CalculateBounds( const Mesh& mesh, const void* vb, const void* ib, uint32_t firstElement, uint32_t elementCount );


}
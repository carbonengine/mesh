// Copyright © 2026 CCP ehf.

#pragma once

#include "cmf.h"

namespace cmf
{

/**
 * @brief Represents a pair of function pointers for bidirectional conversion between a type and raw data stored at a given pointer.
 * @tparam T The type to convert to and from.
 */
template <typename T>
struct ConversionFunction
{
	T ( *to )( const void* ) = nullptr;
	void ( *from )( void*, const T& ) = nullptr;
};


/**
 * @brief Creates a ConversionFunction for converting between two types. The conversion is a simple cast for most types, but for Float_16 
 * it requires special handling to convert to/from float.
 * @tparam From The source type.
 * @tparam To The destination type.
 * @return A ConversionFunction for converting between From and To.
 */
template <typename From, typename To>
inline ConversionFunction<To> MakeConversionFunction()
{
	if constexpr( std::is_same_v<From, Float_16> )
	{
		return {
			[]( const void* data ) { return To( *reinterpret_cast<const From*>( data ) ); },
			[]( void* dest, const To& value ) { *reinterpret_cast<From*>( dest ) = From( float( value ) ); }
		};
	}
	else
	{
		return {
			[]( const void* data ) { return To( *reinterpret_cast<const From*>( data ) ); },
			[]( void* dest, const To& value ) { *reinterpret_cast<From*>( dest ) = From( value ); }
		};
	}
}

/**
 * @brief Creates a bidirectional conversion function for normalized types (UInt8Norm, Int8Norm, UInt16Norm, Int16Norm) to convert between the normalized integer 
 * representation and a floating-point representation in the range [-1, 1] for signed types or [0, 1] for unsigned types.
 * @tparam From The source numeric type to convert from (typically an integer type).
 * @tparam To The destination numeric type to convert to (typically a floating-point type).
 * @return A ConversionFunction for converting between From and To.
 */
template <typename From, typename To>
inline ConversionFunction<To> MakeNormalizedConversionFunction()
{
	return {
		[]( const void* data ) {
			if constexpr( std::is_signed_v<From> )
			{
				return To( std::max( float( *static_cast<const From*>( data ) ) / float( std::numeric_limits<From>::max() ), -1.0f ) );
			}
			else
			{
				return To( float( *static_cast<const From*>( data ) ) / float( std::numeric_limits<From>::max() ) );
			}
		},
		[]( void* dest, const To& value ) {
			if constexpr( std::is_signed_v<From> )
			{
				*static_cast<From*>( dest ) = From( std::round( std::clamp( float( value ), -1.0f, 1.0f ) * float( std::numeric_limits<From>::max() ) ) );
			}
			else
			{
				*static_cast<From*>( dest ) = From( std::round( std::clamp( float( value ), 0.0f, 1.0f ) * float( std::numeric_limits<From>::max() ) ) );
			}
		}
	};
}

/**
 * @brief Gets a scalar conversion function and element size for a given element type.
 * @tparam T The target type for the conversion.
 * @param type The element type that specifies the source type stored in the buffer.
 * @return A pair containing the conversion function from the specified element type to T, and the size in bytes of the source element type. 
 * Returns an empty function and 0 if the element type is not supported.
 */
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
		return { {}, 0 };
	}
}

/**
 * @brief A template adapter struct that allows access to a memory buffer and converts between type T and raw data representations based on a specified ElementType. 
 * Provides bidirectional conversion through operator() for reading and set() for writing.
 * @tparam T The target type for conversion operations.
 */
template <typename T>
struct DeclTypeConverter
{
	DeclTypeConverter( ElementType type, uint8_t count ) :
		m_func( GetScalarConversionFunction<T>( type ).first )
	{
	}

	T operator()( const void* data ) const
	{
		return m_func.to( data );
	}

	void set( void* data, const T& value ) const
	{
		m_func.from( data, value );
	}

	operator bool() const
	{
		return m_func.to != nullptr;
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
		for( uint8_t i = 0; i < m_count; ++i )
		{
			result[i] = m_func.first.to( src );
			src += m_func.second;
		}
		return result;
	}

	void set( void* data, const Vector2& value ) const
	{
		uint8_t* dest = static_cast<uint8_t*>( data );
		for( uint8_t i = 0; i < m_count; ++i )
		{
			m_func.first.from( dest, value[i] );
			dest += m_func.second;
		}
	}

	operator bool() const
	{
		return m_func.first.to != nullptr;
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
			result[i] = m_func.first.to( src );
			src += m_func.second;
		}
		return result;
	}

	void set( void* data, const Vector3& value ) const
	{
		uint8_t* dest = static_cast<uint8_t*>( data );
		for( uint8_t i = 0; i < m_count; ++i )
		{
			m_func.first.from( dest, value[i] );
			dest += m_func.second;
		}
	}

	operator bool() const
	{
		return m_func.first.to != nullptr;
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
			result[i] = m_func.first.to( src );
			src += m_func.second;
		}
		return result;
	}

	void set( void* data, const Vector4& value ) const
	{
		uint8_t* dest = static_cast<uint8_t*>( data );
		for( uint8_t i = 0; i < m_count; ++i )
		{
			m_func.first.from( dest, value[i] );
			dest += m_func.second;
		}
	}

	operator bool() const
	{
		return m_func.first.to != nullptr;
	}

	std::pair<ConversionFunction<float>, size_t> m_func = {};
	uint8_t m_count = 0;
};

template <typename T, size_t N>
struct DeclTypeConverter<std::array<T, N>>
{
	DeclTypeConverter( ElementType type, uint8_t count ) :
		m_func( GetScalarConversionFunction<T>( type ) ),
		m_count( std::min( count, uint8_t( N ) ) )
	{
	}

	std::array<T, N> operator()( const void* data ) const
	{
		std::array<T, N> result = {};
		const uint8_t* src = static_cast<const uint8_t*>( data );
		for( uint8_t i = 0; i < m_count; ++i )
		{
			result[i] = m_func.first.to( src );
			src += m_func.second;
		}
		return result;
	}

	void set( void* data, const std::array<T, N>& value ) const
	{
		uint8_t* dest = static_cast<uint8_t*>( data );
		for( uint8_t i = 0; i < m_count; ++i )
		{
			m_func.first.from( dest, value[i] );
			dest += m_func.second;
		}
	}

	operator bool() const
	{
		return m_func.first.to != nullptr;
	}

	std::pair<ConversionFunction<T>, size_t> m_func = {};
	uint8_t m_count = 0;
};


/**
 * @brief Converts index data between different integer formats for index buffers based on stride size.
 */
struct IndexConverter
{
	IndexConverter( uint32_t stride )
	{
		if( stride == 2 )
		{
			m_func = MakeConversionFunction<uint16_t, uint32_t>();
		}
		else
		{
			m_func = MakeConversionFunction<uint32_t, uint32_t>();
		}
	}

	uint32_t operator()( const void* data ) const
	{
		return m_func.to( data );
	}

	void set( void* data, const uint32_t& value ) const
	{
		m_func.from( data, value );
	}

	operator bool() const
	{
		return m_func.to != nullptr;
	}

	ConversionFunction<uint32_t> m_func = {};
};

}
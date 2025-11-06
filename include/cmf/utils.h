#pragma once

#include <tuple>
#include "cmf.h"
#include "memallocator.h"

namespace cmf
{


template <typename T>
struct ConversionFunction
{
	T ( *to )( const void* ) = nullptr;
	void ( *from )( void*, const T& ) = nullptr;
};


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

template <typename From, typename To>
inline ConversionFunction<To> MakeNormalizedConversionFunction()
{
	return {
		[]( const void* data ) { return To( double( *reinterpret_cast<const From*>( data ) ) / double( std::numeric_limits<From>::max() ) ); },
		[]( void* dest, const To& value ) { *reinterpret_cast<From*>( dest ) = From( value * double( std::numeric_limits<From>::max() ) ); }
	};
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
		return { {}, 0 };
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
        for ( uint8_t i = 0; i < m_count; ++i )
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


template <typename ...Arg>
struct ZipT
{
	ZipT( Arg&&... args ) :
		m_streams{ std::forward<Arg>( args )... }
    {
	}

    std::tuple<Arg...> m_streams;

    template <typename ...It>
    struct Iterator
    {
		Iterator( It&&... args ) :
			m_iterators{  args ... }
		{
		}

        Iterator& operator++() 
        {
			std::apply( []( auto&... it ) { ( ++it, ... ); }, m_iterators );
			return *this;
        }
		bool operator!=( const Iterator& other ) const
		{
			return m_iterators != other.m_iterators;
		}
		auto operator*() const 
        {
			return std::apply( []( auto&... it ) { return std::make_tuple( ( *it )... ); }, m_iterators );
        }
    private:
        std::tuple<It...> m_iterators;
    };

	auto begin() const
	{
		return std::apply( []( auto&... stream ) { return Iterator{ stream.begin()... }; }, m_streams );
	}
	auto end() const
	{
		return std::apply( []( auto&... stream ) { return Iterator{ stream.end()... }; }, m_streams );
	}
};

template <typename... Arg>
inline ZipT<Arg...> Zip( Arg&&... args )
{
    return ZipT<Arg...>( std::forward<Arg>( args )... );
}

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

const VertexElement* FindElement( const Span<VertexElement>& decl, Usage usage, uint8_t usageIndex = 0 );
VertexElement* FindElement( Span<VertexElement>& decl, Usage usage, uint8_t usageIndex = 0 );

uint32_t ComputeCrc32( const void* data, size_t size );


struct ValidationOptions
{
	bool validateCrc = false;
	bool validateHeader = false;

    bool validateMainData = false;
};

using ValidationResult = std::pair<bool, ValidationOptions>;

ValidationResult ValidateFile( const void* data, size_t size, const ValidationOptions& options );

}
#pragma once

#include "carbon_mesh_export.h"
#include <cstdint>
#include <type_traits>

namespace cmf
{

constexpr uint32_t FILE_SIGNATURE = 0x66666D63;
}

#include "v1.h"

namespace cmf
{

using namespace v1;


template <typename T, typename = void>
struct IsCmfDataType : std::false_type
{
};

template <typename T>
struct IsCmfDataType<T, decltype( std::declval<T>().TypeName, void() )> : std::true_type
{
};

template <typename U, typename T>
constexpr inline typename std::enable_if<IsCmfDataType<U>::value>::type EnumerateMembers( U& data, T&& visitor )
{
	data.EnumerateMembers( visitor );
}

template <typename U, typename T>
constexpr inline typename std::enable_if<!IsCmfDataType<U>::value>::type EnumerateMembers( U& data, T&& visitor )
{
}

template <typename U, typename T>
constexpr inline void EnumerateChildren( U& data, T&& visitor )
{
	if constexpr ( std::is_base_of_v<SpanRepr, U> )
    {
        for( auto& element : data )
        {
			visitor( data, element, "" );
        }
    }
    else
    {
        EnumerateMembers( data, [&visitor]( auto&&, auto& value, const char* ) {
            EnumerateChildren( value, visitor );
        } );
	}
}

template <typename T>
void OffsetsToPointers( T& data )
{
	if constexpr( std::is_base_of_v<SpanRepr, typename std::remove_reference<T>::type> )
	{
		if( ( data.offset & 1 ) != 0 )
		{
			data.ptr = reinterpret_cast<uint8_t*>( &data.ptr ) + ( data.offset & ~1ll );
		}
		for( auto& element : data )
		{
			OffsetsToPointers( element );
		}
	}
	else
	{
		EnumerateMembers( data, []( auto&&, auto& value, const char* ) {
			OffsetsToPointers( value );
		} );
	}
}


template <typename T>
void PointersToOffsets( T& data )
{
	if constexpr( std::is_base_of_v<SpanRepr, T> )
	{
		for( auto& element : data )
		{
			PointersToOffsets( element );
		}
		if( ( data.offset & 1 ) == 0 )
		{
			data.offset = ( static_cast<uint8_t*>( data.ptr ) - reinterpret_cast<uint8_t*>( &data.ptr ) ) | 1;
		}
	}
	else
	{
		EnumerateMembers( data, []( auto&&, auto& value, const char* ) {
			PointersToOffsets( value );
		} );
	}
}

}
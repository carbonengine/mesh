#pragma once

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
void OffsetsToPointers( T& data, const void* base )
{
	if constexpr( std::is_base_of_v<SpanRepr, typename std::remove_reference<T>::type> )
	{
		data.ptr = reinterpret_cast<void*>( reinterpret_cast<uintptr_t>( base ) + data.offset );
		for( auto& element : data )
		{
			OffsetsToPointers( element, base );
		}
	}
	else
	{
		EnumerateMembers( data, [base]( auto&&, auto& value, const char* ) {
			OffsetsToPointers( value, base );
		} );
	}
}


template <typename T>
void PointersToOffsets( T& data, const void* base )
{
	if constexpr( std::is_base_of_v<SpanRepr, T> )
	{
		for( auto& element : data )
		{
			PointersToOffsets( element, base );
		}
		data.offset = static_cast<uint8_t*>( data.ptr ) - static_cast<const uint8_t*>( base );
	}
	else
	{
		EnumerateMembers( data, [base]( auto&&, auto& value, const char* ) {
			PointersToOffsets( value, base );
		} );
	}
}

}
// Copyright © 2026 CCP ehf.

#pragma once

#include "cmf/cmf.h"

#include <cstdint>
#include <string>
#include <type_traits>

// Internal validation helpers shared by the CMF and CAN file validators.

namespace cmf::detail
{

/** Recursively checks that every span reachable from value is aligned and lies within [base, base + totalSize).
 * Returns an empty string when valid, otherwise a description of the first problem found. */
template <typename T>
std::string AreSpanPointersValid( const T& value, const void* base, size_t totalSize ) // NOLINT(readability-identifier-naming): clang-tidy classifies function templates as Default (camelBack); name follows FunctionCase
{
	if constexpr( std::is_base_of_v<cmf::SpanRepr, T> )
	{
		// Total size must be multiple of element size
		if( value.byteSize % sizeof( typename T::value_type ) != 0 )
		{
			return "Total size must be multiple of element size";
		}
		// Empty spans are valid
		if( value.size() == 0 )
		{
			return "";
		}
		// Pointer must be aligned to element size
		if( reinterpret_cast<uintptr_t>( value.data() ) % alignof( typename T::value_type ) != 0 ) // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast): alignment check needs the address as an integer
		{
			return "Pointer must be aligned to element size";
		}

		// Pointer must be within the base + totalSize range
		const auto* end = reinterpret_cast<const uint8_t*>( value.end() ); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast): byte address for the range check
		const auto* limit = static_cast<const uint8_t*>( base ) + totalSize;
		if( value.data() < base || end > limit )
		{
			return "Pointer must be within the base + totalSize range";
		}

		size_t index = 0;
		for( const auto& element : value )
		{
			auto result = AreSpanPointersValid( element, base, totalSize );
			if( !result.empty() )
			{
				return "[" + std::to_string( index ) + "] " + result;
			}
			++index;
		}
		return "";
	}
	else
	{
		std::string errorMessage;
		// EnumerateMembers only has a non-const overload; the visitor below does not modify anything.
		cmf::EnumerateMembers( const_cast<T&>( value ), [&errorMessage, base, totalSize]( auto&&, auto& value, const char* memberName ) { // NOLINT(cppcoreguidelines-pro-type-const-cast)
			if( errorMessage.empty() )
			{
				auto result = AreSpanPointersValid( value, base, totalSize );
				if( !result.empty() )
				{
					errorMessage = std::string( memberName ) + " " + result;
				}
			}
		} );
		return errorMessage;
	}
}

}

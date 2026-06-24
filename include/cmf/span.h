// Copyright © 2025 CCP ehf.

#pragma once

#include <cstdint>
#include <string>

namespace cmf
{

/**
 * @brief Memory representation for cmf::Span. The first element of the span is pointed with `ptr`, or using `offset` from the location of the ptr field itself.
 * This allows cmf::Span to be used in data structures loaded directly from a file without needing to convert offsets to pointers first, and also allows cmf::Span 
 * to be used in memory buffers that may be relocated without needing to update internal pointers. The least significant bit of the offset field is used as a flag 
 * to indicate whether the offset-based addressing is used (if set) or pointer-based addressing is used (if not set). When using offset-based addressing, the actual 
 * offset value is stored in the remaining bits of the offset field, and the pointer can be calculated by adding this offset to the address of the ptr field itself. 
 * When using pointer-based addressing, the ptr field directly contains the pointer to the first element of the span.
 */
struct SpanRepr
{
	union
	{
		int64_t offset = 0;
		void* ptr;
	};
	uint64_t byteSize = 0;
};

// NOLINTBEGIN(readability-identifier-naming, cppcoreguidelines-pro-type-reinterpret-cast, cppcoreguidelines-pro-type-union-access)

/**
 * @brief A view over a contiguous sequence of objects of type T, similar to std::span in C++20. It provides a lightweight, 
 * non-owning reference to an array of elements, allowing for easy access and iteration over the elements without copying.
 * @tparam T The type of elements in the span.
 */
template <typename T>
struct Span : public SpanRepr
{
	using value_type = T;

	[[nodiscard]] T* begin()
	{
		if( offset & 1 )
		{
			return reinterpret_cast<T*>( reinterpret_cast<uint8_t*>( &this->ptr ) + ( this->offset & ~1ll ) );
		}
		return static_cast<T*>( this->ptr );
	}
	[[nodiscard]] const T* begin() const
	{
		if( offset & 1 )
		{
			return reinterpret_cast<const T*>( reinterpret_cast<const uint8_t*>( &this->ptr ) + ( this->offset & ~1ll ) );
		}
		return static_cast<const T*>( this->ptr );
	}
	[[nodiscard]] T* end()
	{
		return this->begin() + this->size();
	}
	[[nodiscard]] const T* end() const
	{
		return this->begin() + this->size();
	}
	[[nodiscard]] T* data()
	{
		return begin();
	}
	[[nodiscard]] const T* data() const
	{
		return begin();
	}
	[[nodiscard]] std::size_t size() const
	{
		return this->byteSize / sizeof( T );
	}
	[[nodiscard]] bool empty() const
	{
		return this->byteSize == 0;
	}
	[[nodiscard]] T& operator[]( std::size_t index )
	{
		return data()[index];
	}
	[[nodiscard]] const T& operator[]( std::size_t index ) const
	{
		return data()[index];
	}
};
// NOLINTEND(readability-identifier-naming, cppcoreguidelines-pro-type-reinterpret-cast, cppcoreguidelines-pro-type-union-access)

using String = Span<char>;

inline std::string ToStdString( const String& str )
{
	return { str.begin(), str.end() };
}

inline std::string_view ToStdStringView( const String& str )
{
	return { str.begin(), size_t( str.end() - str.begin() ) };
}

inline bool operator==( const String& a, const String& b )
{
	return a.size() == b.size() && std::equal( a.begin(), a.end(), b.begin() );
}

inline bool operator!=( const String& a, const String& b )
{
	return !( a == b );
}


}

#pragma once

#include <cstdint>
#include <string>

namespace cmf
{

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

inline bool operator==( const String& a, const String& b )
{
	return a.size() == b.size() && std::equal( a.begin(), a.end(), b.begin() );
}

inline bool operator!=( const String& a, const String& b )
{
	return !( a == b );
}


}

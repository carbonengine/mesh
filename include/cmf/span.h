#pragma once

#include <cstdint>

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

template <typename T>
struct Span : public SpanRepr
{
	using value_type = T;

	T* begin()
	{
		if( offset & 1 )
		{
			return reinterpret_cast<T*>( reinterpret_cast<uint8_t*>( &this->ptr ) + ( this->offset & ~1ll ) );
		}
		return reinterpret_cast<T*>( this->ptr );
	}
	const T* begin() const
	{
		if( offset & 1 )
		{
			return reinterpret_cast<const T*>( reinterpret_cast<const uint8_t*>( &this->ptr ) + ( this->offset & ~1ll ) );
		}
		return reinterpret_cast<const T*>( this->ptr );
	}
	T* end()
	{
		return this->begin() + this->size();
	}
	const T* end() const
	{
		return this->begin() + this->size();
	}
	T* data()
	{
		return begin();
	}
	const T* data() const
	{
		return begin();
	}
	std::size_t size() const
	{
		return this->byteSize / sizeof( T );
	}
	bool empty() const
	{
		return this->byteSize == 0;
	}
	T& operator[]( std::size_t index )
	{
		return data()[index];
	}
	const T& operator[]( std::size_t index ) const
	{
		return data()[index];
	}
};

using String = Span<char>;

}

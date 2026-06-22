// Copyright © 2025 CCP ehf.

#pragma once

#include <tuple>
#include <string>
#include "cmf.h"

namespace cmf
{


template <typename... Arg>
struct ZipT
{
	ZipT( Arg&&... args ) :
		m_streams{ std::forward<Arg>( args )... }
	{
	}

	std::tuple<Arg...> m_streams;

	template <typename... It>
	struct Iterator
	{
		Iterator( It&&... args ) :
			m_iterators{ args... }
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

CARBON_MESH_EXPORT uint32_t ComputeCrc32( const void* data, size_t size );


struct ValidationOptions
{
	bool validateCrc = false;
	bool validateHeader = false;

	bool validateMainData = false;
	bool validateBuffers = false;
};

struct ValidationResult
{
	bool valid = false;
	std::string error;

	explicit operator bool() const
	{
		return valid;
	}
};

CARBON_MESH_EXPORT ValidationResult ValidateFile( const void* data, size_t size, const ValidationOptions& options );

}
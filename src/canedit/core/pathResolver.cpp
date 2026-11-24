// Copyright © 2026 CCP ehf.

#include "pathResolver.h"

#include <algorithm>
#include <cctype>

namespace canedit
{
namespace
{

bool IsPrefixCharacter( char c )
{
	return std::isalnum( static_cast<unsigned char>( c ) ) != 0 || c == '_' || c == '-' || c == '.';
}

// Returns true if path is inside directory (or equal to it) and sets relative to the remaining part
bool IsInside( const std::filesystem::path& path, const std::filesystem::path& directory, std::filesystem::path& relative )
{
	const auto pathText = path.lexically_normal().generic_string();
	auto directoryText = directory.lexically_normal().generic_string();
	while( !directoryText.empty() && directoryText.back() == '/' )
	{
		directoryText.pop_back();
	}
	if( directoryText.empty() || pathText.size() <= directoryText.size() )
	{
		return false;
	}
	const auto compare = []( char a, char b ) {
#ifdef _WIN32
		return std::tolower( static_cast<unsigned char>( a ) ) == std::tolower( static_cast<unsigned char>( b ) );
#else
		return a == b;
#endif
	};
	if( !std::equal( directoryText.begin(), directoryText.end(), pathText.begin(), compare ) || pathText[directoryText.size()] != '/' )
	{
		return false;
	}
	relative = std::filesystem::path( pathText.substr( directoryText.size() + 1 ) );
	return true;
}

}

void PathResolver::SetPrefixes( std::vector<PathPrefix> prefixes )
{
	m_prefixes = std::move( prefixes );
}

const std::vector<PathPrefix>& PathResolver::GetPrefixes() const
{
	return m_prefixes;
}

bool PathResolver::SplitPrefix( std::string_view reference, std::string_view& name, std::string_view& rest )
{
	const auto colon = reference.find( ':' );
	if( colon == std::string_view::npos || colon < MIN_PREFIX_LENGTH )
	{
		return false;
	}
	const auto candidate = reference.substr( 0, colon );
	if( !std::all_of( candidate.begin(), candidate.end(), IsPrefixCharacter ) )
	{
		return false;
	}
	name = candidate;
	rest = reference.substr( colon + 1 );
	return true;
}

std::string PathResolver::ValidatePrefixName( std::string_view name )
{
	if( name.size() < MIN_PREFIX_LENGTH )
	{
		return "Prefix names must have at least " + std::to_string( MIN_PREFIX_LENGTH ) + " characters";
	}
	if( !std::all_of( name.begin(), name.end(), IsPrefixCharacter ) )
	{
		return "Prefix names may only contain letters, digits, '_', '-' and '.'";
	}
	return {};
}

std::optional<std::filesystem::path> PathResolver::Resolve( std::string_view reference, std::string* error ) const
{
	std::string_view name;
	std::string_view rest;
	if( !SplitPrefix( reference, name, rest ) )
	{
		return std::filesystem::u8path( reference );
	}
	const auto it = std::find_if( m_prefixes.begin(), m_prefixes.end(), [name]( const PathPrefix& prefix ) { return prefix.name == name; } );
	if( it == m_prefixes.end() )
	{
		if( error != nullptr )
		{
			*error = "Unknown path prefix '" + std::string( name ) + "'";
		}
		return std::nullopt;
	}
	while( !rest.empty() && ( rest.front() == '/' || rest.front() == '\\' ) )
	{
		rest.remove_prefix( 1 );
	}
	return ( std::filesystem::u8path( it->directory ) / std::filesystem::u8path( rest ) ).lexically_normal();
}

std::string PathResolver::MakeReference( const std::filesystem::path& path ) const
{
	const PathPrefix* best = nullptr;
	std::filesystem::path bestRelative;
	size_t bestLength = 0;
	for( const auto& prefix : m_prefixes )
	{
		std::filesystem::path relative;
		const auto directory = std::filesystem::u8path( prefix.directory );
		const auto length = directory.lexically_normal().generic_string().size();
		if( length > bestLength && IsInside( path, directory, relative ) )
		{
			best = &prefix;
			bestRelative = relative;
			bestLength = length;
		}
	}
	if( best == nullptr )
	{
		return path.lexically_normal().generic_u8string();
	}
	return best->name + ":/" + bestRelative.generic_u8string();
}

}

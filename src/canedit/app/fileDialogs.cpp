// Copyright © 2026 CCP ehf.

#include "fileDialogs.h"

#include <tinyfiledialogs/tinyfiledialogs.h>

#include <array>
#include <filesystem>

namespace canedit::dialogs
{
namespace
{

std::optional<std::string> ToOptional( const char* path )
{
	if( path == nullptr || *path == '\0' )
	{
		return std::nullopt;
	}
	return std::string( path );
}

// tinyfiledialogs rejects some characters in titles and messages
std::string Sanitize( std::string text )
{
	for( auto& c : text )
	{
		if( c == '"' || c == '\'' || c == '`' )
		{
			c = ' ';
		}
	}
	return text;
}

}

std::optional<std::string> OpenNetwork()
{
	const std::array<const char*, 1> filter = { "*.can" };
	return ToOptional( tinyfd_openFileDialog( "Open Animation Network", nullptr, static_cast<int>( filter.size() ), filter.data(), "Animation Networks (*.can)", 0 ) );
}

std::optional<std::string> SaveNetwork( const std::string& defaultPath )
{
	const std::array<const char*, 1> filter = { "*.can" };
	auto path = ToOptional( tinyfd_saveFileDialog( "Save Animation Network", defaultPath.empty() ? nullptr : defaultPath.c_str(), static_cast<int>( filter.size() ), filter.data(), "Animation Networks (*.can)" ) );
	if( path && !std::filesystem::u8path( *path ).has_extension() )
	{
		*path += ".can";
	}
	return path;
}

std::optional<std::string> OpenCmf( const char* title )
{
	const std::array<const char*, 1> filter = { "*.cmf" };
	return ToOptional( tinyfd_openFileDialog( title, nullptr, static_cast<int>( filter.size() ), filter.data(), "CMF Files (*.cmf)", 0 ) );
}

std::optional<std::string> SelectDirectory( const char* title, const std::string& defaultPath )
{
	return ToOptional( tinyfd_selectFolderDialog( title, defaultPath.empty() ? nullptr : defaultPath.c_str() ) );
}

Answer AskSaveChanges( const std::string& documentName )
{
	const auto message = Sanitize( "Save changes to " + documentName + "?" );
	switch( tinyfd_messageBox( "CanEdit", message.c_str(), "yesnocancel", "question", 1 ) )
	{
	case 1:
		return Answer::Yes;
	case 2:
		return Answer::No;
	default:
		return Answer::Cancel;
	}
}

void ShowError( const std::string& message )
{
	const auto text = Sanitize( message );
	tinyfd_messageBox( "CanEdit", text.c_str(), "ok", "error", 1 );
}

}

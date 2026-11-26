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

// Asks for a file to write with a single extension, which is added if the chosen name has none
std::optional<std::string> SaveFile( const char* title, const std::string& defaultPath, const char* pattern, const char* description )
{
	const std::array<const char*, 1> filter = { pattern };
	auto path = ToOptional( tinyfd_saveFileDialog( title, defaultPath.empty() ? nullptr : defaultPath.c_str(), static_cast<int>( filter.size() ), filter.data(), description ) );
	if( path && !std::filesystem::u8path( *path ).has_extension() )
	{
		// The pattern is "*.extension"
		*path += std::string( pattern ).substr( 1 );
	}
	return path;
}

}

std::optional<std::string> OpenNetwork()
{
	const std::array<const char*, 1> filter = { "*.cane" };
	return ToOptional( tinyfd_openFileDialog( "Open Animation Network", nullptr, static_cast<int>( filter.size() ), filter.data(), "Animation Network Documents (*.cane)", 0 ) );
}

std::optional<std::string> SaveNetwork( const std::string& defaultPath )
{
	return SaveFile( "Save Animation Network", defaultPath, "*.cane", "Animation Network Documents (*.cane)" );
}

std::optional<std::string> ExportNetwork( const std::string& defaultPath )
{
	return SaveFile( "Export Animation Network", defaultPath, "*.can", "Animation Networks (*.can)" );
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

bool AskYesNo( const std::string& message )
{
	const auto text = Sanitize( message );
	return tinyfd_messageBox( "CanEdit", text.c_str(), "yesno", "question", 1 ) == 1;
}

void ShowError( const std::string& message )
{
	const auto text = Sanitize( message );
	tinyfd_messageBox( "CanEdit", text.c_str(), "ok", "error", 1 );
}

void ShowWarning( const std::string& message )
{
	const auto text = Sanitize( message );
	tinyfd_messageBox( "CanEdit", text.c_str(), "ok", "warning", 1 );
}

}

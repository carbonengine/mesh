// Copyright © 2026 CCP ehf.

#include "settings.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>

namespace canedit
{
namespace
{

std::filesystem::path GetEnvironmentPath( const char* name )
{
#ifdef _WIN32
	char* value = nullptr;
	size_t size = 0;
	if( _dupenv_s( &value, &size, name ) != 0 || value == nullptr )
	{
		return {};
	}
	std::filesystem::path result = std::filesystem::u8path( value );
	free( value ); // NOLINT(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory): _dupenv_s allocates with malloc
	return result;
#else
	const char* value = std::getenv( name ); // NOLINT(concurrency-mt-unsafe): read once at startup
	return value != nullptr ? std::filesystem::path( value ) : std::filesystem::path();
#endif
}

void ReadFloat( const nlohmann::json& json, const char* key, float& value )
{
	const auto it = json.find( key );
	if( it != json.end() && it->is_number() )
	{
		value = it->get<float>();
	}
}

}

void EditorSettings::AddRecentFile( const std::string& path )
{
	recentFiles.erase( std::remove( recentFiles.begin(), recentFiles.end(), path ), recentFiles.end() );
	recentFiles.insert( recentFiles.begin(), path );
	if( recentFiles.size() > MAX_RECENT_FILES )
	{
		recentFiles.resize( MAX_RECENT_FILES );
	}
}

std::filesystem::path GetDefaultSettingsPath()
{
#ifdef _WIN32
	auto base = GetEnvironmentPath( "APPDATA" );
#elif defined( __APPLE__ )
	auto base = GetEnvironmentPath( "HOME" ) / "Library" / "Application Support";
#else
	auto base = GetEnvironmentPath( "XDG_CONFIG_HOME" );
	if( base.empty() )
	{
		base = GetEnvironmentPath( "HOME" ) / ".config";
	}
#endif
	if( base.empty() )
	{
		base = std::filesystem::current_path();
	}
	return base / "CanEdit" / "settings.json";
}

std::string SettingsToJson( const EditorSettings& settings )
{
	nlohmann::json json;
	auto& prefixes = json["prefixes"];
	prefixes = nlohmann::json::array();
	for( const auto& prefix : settings.prefixes )
	{
		prefixes.push_back( { { "name", prefix.name }, { "directory", prefix.directory } } );
	}
	json["recentFiles"] = settings.recentFiles;
	json["uiScale"] = settings.uiScale;
	json["leftPanelWidth"] = settings.leftPanelWidth;
	json["rightPanelWidth"] = settings.rightPanelWidth;
	json["previewHeight"] = settings.previewHeight;
	json["parametersHeight"] = settings.parametersHeight;
	json["paletteHeight"] = settings.paletteHeight;
	return json.dump( 1, '\t' );
}

bool SettingsFromJson( const std::string& text, EditorSettings& settings, std::string& error )
{
	const auto json = nlohmann::json::parse( text, nullptr, false );
	if( json.is_discarded() || !json.is_object() )
	{
		error = "Settings are not valid JSON";
		return false;
	}
	EditorSettings result;
	const auto prefixes = json.find( "prefixes" );
	if( prefixes != json.end() && prefixes->is_array() )
	{
		for( const auto& prefix : *prefixes )
		{
			if( prefix.is_object() )
			{
				result.prefixes.push_back( { prefix.value( "name", "" ), prefix.value( "directory", "" ) } );
			}
		}
	}
	const auto recent = json.find( "recentFiles" );
	if( recent != json.end() && recent->is_array() )
	{
		for( const auto& file : *recent )
		{
			if( file.is_string() )
			{
				result.recentFiles.push_back( file.get<std::string>() );
			}
		}
	}
	ReadFloat( json, "uiScale", result.uiScale );
	ReadFloat( json, "leftPanelWidth", result.leftPanelWidth );
	ReadFloat( json, "rightPanelWidth", result.rightPanelWidth );
	ReadFloat( json, "previewHeight", result.previewHeight );
	ReadFloat( json, "parametersHeight", result.parametersHeight );
	ReadFloat( json, "paletteHeight", result.paletteHeight );
	settings = std::move( result );
	return true;
}

bool LoadSettings( const std::filesystem::path& path, EditorSettings& settings, std::string& error )
{
	std::error_code ec;
	if( !std::filesystem::exists( path, ec ) )
	{
		settings = EditorSettings{};
		return true;
	}
	const std::ifstream stream( path );
	if( !stream )
	{
		error = "Cannot read '" + path.u8string() + "'";
		return false;
	}
	std::stringstream text;
	text << stream.rdbuf();
	return SettingsFromJson( text.str(), settings, error );
}

bool SaveSettings( const std::filesystem::path& path, const EditorSettings& settings, std::string& error )
{
	std::error_code ec;
	std::filesystem::create_directories( path.parent_path(), ec );
	std::ofstream stream( path, std::ios::trunc );
	if( !stream )
	{
		error = "Cannot write '" + path.u8string() + "'";
		return false;
	}
	stream << SettingsToJson( settings );
	return static_cast<bool>( stream );
}

}

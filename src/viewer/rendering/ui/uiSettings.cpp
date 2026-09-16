// Copyright © 2026 CCP ehf.

#include "uiSettings.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <algorithm>
#include <imgui.h>
#include <imgui_internal.h> // ImGuiSettingsHandler
#include "log.h"
#include "uiConsts.h"

#if defined( _WIN32 )
#include <direct.h>
#define MAKE_DIRECTORY( path ) _mkdir( path )
#else
#include <sys/stat.h>
#define MAKE_DIRECTORY( path ) mkdir( path, 0755 )
#endif

// NOLINTBEGIN(cppcoreguidelines-pro-type-vararg)

namespace
{
const char* const SETTINGS_TYPE_NAME = "CarbonMeshViewer"; // ini section is [CarbonMeshViewer][Settings]
const char* const SETTINGS_ENTRY_NAME = "Settings";
const char* const INI_FILE_NAME = "imgui.ini";

// io.IniFilename is a raw pointer, so the path has to outlive the ImGui context
std::string g_iniFilePath;

void CreateDirectories( const std::string& path )
{
	for( size_t pos = path.find_first_of( "/\\", 1 ); pos != std::string::npos; pos = path.find_first_of( "/\\", pos + 1 ) )
	{
		MAKE_DIRECTORY( path.substr( 0, pos ).c_str() );
	}
	MAKE_DIRECTORY( path.c_str() );
}

std::string GetEnvironmentValue( const char* name )
{
#if defined( _MSC_VER )
	char* value = nullptr;
	size_t length = 0;
	if( _dupenv_s( &value, &length, name ) != 0 || value == nullptr )
	{
		return "";
	}
	std::string result( value );
	std::free( value );
	return result;
#else
	const char* value = std::getenv( name );
	return value != nullptr ? std::string( value ) : "";
#endif
}

// empty (= working directory) if the environment does not tell us where the user directory is
std::string GetSettingsDirectory()
{
#if defined( _WIN32 )
	const std::string base = GetEnvironmentValue( "LOCALAPPDATA" );
	const char* subDirectory = "\\FenrisCreations\\CarbonMeshViewer";
#elif defined( __APPLE__ )
	const std::string base = GetEnvironmentValue( "HOME" );
	const char* subDirectory = "/Library/Application Support/FenrisCreations/CarbonMeshViewer";
#else
	const std::string base = GetEnvironmentValue( "HOME" );
	const char* subDirectory = "/.config/FenrisCreations/CarbonMeshViewer";
#endif
	if( base.empty() )
	{
		return "";
	}
	return base + subDirectory;
}

AppState& GetAppState( ImGuiSettingsHandler* handler )
{
	return *static_cast<AppState*>( handler->UserData );
}

void* ReadOpen( ImGuiContext*, ImGuiSettingsHandler* handler, const char* name )
{
	return std::strcmp( name, SETTINGS_ENTRY_NAME ) == 0 ? handler->UserData : nullptr;
}

void ReadLine( ImGuiContext*, ImGuiSettingsHandler* handler, void*, const char* line )
{
	const char* const key = "UiScale=";
	const size_t keyLength = std::strlen( key );
	if( std::strncmp( line, key, keyLength ) == 0 )
	{
		char* end = nullptr;
		const float uiScale = std::strtof( line + keyLength, &end );
		if( end != line + keyLength )
		{
			GetAppState( handler ).uiScale.SetValueNoCallback( std::clamp( uiScale, UiConsts::MIN_UI_SCALE, UiConsts::MAX_UI_SCALE ) );
		}
	}
}

void WriteAll( ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* outBuffer )
{
	outBuffer->appendf( "[%s][%s]\n", handler->TypeName, SETTINGS_ENTRY_NAME );
	outBuffer->appendf( "UiScale=%.3f\n", GetAppState( handler ).uiScale.GetValue() );
	outBuffer->append( "\n" );
}
}

namespace UiSettings
{
void Initialize( AppState& appState )
{
	const std::string directory = GetSettingsDirectory();
	if( directory.empty() )
	{
		g_iniFilePath = INI_FILE_NAME;
	}
	else
	{
		CreateDirectories( directory );
#if defined( _WIN32 )
		g_iniFilePath = directory + "\\" + INI_FILE_NAME;
#else
		g_iniFilePath = directory + "/" + INI_FILE_NAME;
#endif
	}
	ImGui::GetIO().IniFilename = g_iniFilePath.c_str();
	Log::Info( "Using settings file %s", g_iniFilePath.c_str() );

	ImGuiSettingsHandler handler;
	handler.TypeName = SETTINGS_TYPE_NAME;
	handler.TypeHash = ImHashStr( SETTINGS_TYPE_NAME );
	handler.ReadOpenFn = ReadOpen;
	handler.ReadLineFn = ReadLine;
	handler.WriteAllFn = WriteAll;
	handler.UserData = &appState;
	ImGui::AddSettingsHandler( &handler );

	// load now rather than lazily on the first frame
	ImGui::LoadIniSettingsFromDisk( g_iniFilePath.c_str() );
}

void MarkDirty()
{
	ImGui::MarkIniSettingsDirty();
}

void Save()
{
	if( !g_iniFilePath.empty() )
	{
		ImGui::SaveIniSettingsToDisk( g_iniFilePath.c_str() );
	}
}

const std::string& GetIniFilePath()
{
	return g_iniFilePath;
}
}

// NOLINTEND(cppcoreguidelines-pro-type-vararg)

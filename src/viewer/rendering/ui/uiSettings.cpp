// Copyright © 2026 CCP ehf.

#include "uiSettings.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <imgui.h>
#include <imgui_internal.h> // ImGuiSettingsHandler
#include "log.h"
#include "uiConsts.h"

// NOLINTBEGIN(cppcoreguidelines-pro-type-vararg)

namespace
{
const char* const SETTINGS_TYPE_NAME = "CarbonMeshViewer"; // ini section is [CarbonMeshViewer][Settings]
const char* const SETTINGS_ENTRY_NAME = "Settings";

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
	ImGuiSettingsHandler handler;
	handler.TypeName = SETTINGS_TYPE_NAME;
	handler.TypeHash = ImHashStr( SETTINGS_TYPE_NAME );
	handler.ReadOpenFn = ReadOpen;
	handler.ReadLineFn = ReadLine;
	handler.WriteAllFn = WriteAll;
	handler.UserData = &appState;
	ImGui::AddSettingsHandler( &handler );

	// load now rather than lazily on the first frame
	const char* iniFile = ImGui::GetIO().IniFilename;
	Log::Info( "Using settings file %s", iniFile );
	ImGui::LoadIniSettingsFromDisk( iniFile );
}

void MarkDirty()
{
	ImGui::MarkIniSettingsDirty();
}

void Save()
{
	ImGui::SaveIniSettingsToDisk( ImGui::GetIO().IniFilename );
}
}

// NOLINTEND(cppcoreguidelines-pro-type-vararg)

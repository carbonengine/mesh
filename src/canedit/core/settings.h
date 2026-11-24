// Copyright © 2026 CCP ehf.

#pragma once

#include "pathResolver.h"

#include <filesystem>
#include <string>
#include <vector>

namespace canedit
{

/** User settings of the editor, stored as JSON in the user configuration directory. */
struct EditorSettings
{
	static constexpr size_t MAX_RECENT_FILES = 10;
	static constexpr float DEFAULT_LEFT_PANEL_WIDTH = 320.f;
	static constexpr float DEFAULT_RIGHT_PANEL_WIDTH = 420.f;
	static constexpr float DEFAULT_PREVIEW_HEIGHT = 380.f;
	static constexpr float DEFAULT_PARAMETERS_HEIGHT = 260.f;
	static constexpr float DEFAULT_PALETTE_HEIGHT = 280.f;

	std::vector<PathPrefix> prefixes;
	std::vector<std::string> recentFiles;
	/** UI scale; 0 uses the scale of the monitor. */
	float uiScale = 0.f;
	/** Panel sizes in unscaled pixels. */
	float leftPanelWidth = DEFAULT_LEFT_PANEL_WIDTH;
	float rightPanelWidth = DEFAULT_RIGHT_PANEL_WIDTH;
	float previewHeight = DEFAULT_PREVIEW_HEIGHT;
	float parametersHeight = DEFAULT_PARAMETERS_HEIGHT;
	float paletteHeight = DEFAULT_PALETTE_HEIGHT;

	/** Moves a file to the front of the recent files. */
	void AddRecentFile( const std::string& path );
};

/** Returns the settings file location: %APPDATA%/CanEdit/settings.json on Windows,
* ~/Library/Application Support/CanEdit/settings.json on macOS and ~/.config/CanEdit/settings.json elsewhere. */
[[nodiscard]] std::filesystem::path GetDefaultSettingsPath();

/** Reads settings. A missing file gives default settings; returns false with an error for unreadable files. */
bool LoadSettings( const std::filesystem::path& path, EditorSettings& settings, std::string& error );
/** Writes settings, creating the directory if needed. */
bool SaveSettings( const std::filesystem::path& path, const EditorSettings& settings, std::string& error );

/** Converts settings to and from JSON text. */
[[nodiscard]] std::string SettingsToJson( const EditorSettings& settings );
bool SettingsFromJson( const std::string& text, EditorSettings& settings, std::string& error );

}

// Copyright © 2026 CCP ehf.

#include "settings.h"
#include "testutils.h"

namespace canedittest
{
namespace
{

constexpr float SCALE = 1.5f;
constexpr float WIDTH = 123.f;

TEST( Settings, RoundTripThroughJson )
{
	canedit::EditorSettings settings;
	settings.prefixes = { { "res", "c:/content" }, { "art", "/art" } };
	settings.recentFiles = { "a.cane", "b.cane" };
	settings.uiScale = SCALE;
	settings.leftPanelWidth = WIDTH;

	canedit::EditorSettings loaded;
	std::string error;
	ASSERT_TRUE( canedit::SettingsFromJson( canedit::SettingsToJson( settings ), loaded, error ) ) << error;
	EXPECT_EQ( loaded.prefixes, settings.prefixes );
	EXPECT_EQ( loaded.recentFiles, settings.recentFiles );
	EXPECT_EQ( loaded.uiScale, SCALE );
	EXPECT_EQ( loaded.leftPanelWidth, WIDTH );
	EXPECT_EQ( loaded.rightPanelWidth, canedit::EditorSettings::DEFAULT_RIGHT_PANEL_WIDTH );
}

TEST( Settings, MissingFilesGiveDefaults )
{
	const TemporaryDirectory directory;
	canedit::EditorSettings settings;
	settings.uiScale = SCALE;
	std::string error;
	ASSERT_TRUE( canedit::LoadSettings( directory.GetPath() / "missing.json", settings, error ) );
	EXPECT_EQ( settings.uiScale, 0.f );

	const auto path = directory.GetPath() / "sub" / "settings.json";
	settings.uiScale = SCALE;
	ASSERT_TRUE( canedit::SaveSettings( path, settings, error ) ) << error;
	canedit::EditorSettings loaded;
	ASSERT_TRUE( canedit::LoadSettings( path, loaded, error ) ) << error;
	EXPECT_EQ( loaded.uiScale, SCALE );
}

TEST( Settings, InvalidJsonIsAnError )
{
	canedit::EditorSettings settings;
	std::string error;
	EXPECT_FALSE( canedit::SettingsFromJson( "{ nope", settings, error ) );
	EXPECT_FALSE( error.empty() );
}

TEST( Settings, RecentFilesAreMostRecentFirstAndLimited )
{
	canedit::EditorSettings settings;
	for( size_t i = 0; i < canedit::EditorSettings::MAX_RECENT_FILES + 2; ++i )
	{
		settings.AddRecentFile( std::to_string( i ) );
	}
	EXPECT_EQ( settings.recentFiles.size(), canedit::EditorSettings::MAX_RECENT_FILES );
	settings.AddRecentFile( "5" );
	EXPECT_EQ( settings.recentFiles.front(), "5" );
	EXPECT_EQ( std::count( settings.recentFiles.begin(), settings.recentFiles.end(), "5" ), 1 );
}

}
}

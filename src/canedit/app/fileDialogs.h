// Copyright © 2026 CCP ehf.

#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace canedit::dialogs
{

/** Asks for an animation network document (.cane). */
std::optional<std::string> OpenNetwork();
/** Asks where to save an animation network document (.cane). */
std::optional<std::string> SaveNetwork( const std::string& defaultPath );
/** Asks where to export an animation network in the CAN format. */
std::optional<std::string> ExportNetwork( const std::string& defaultPath );
/** Asks for a CMF file, for example a skeleton, animation or preview mesh. */
std::optional<std::string> OpenCmf( const char* title );

/** Asks for a directory. */
std::optional<std::string> SelectDirectory( const char* title, const std::string& defaultPath );

enum class Answer : uint8_t
{
	Yes,
	No,
	Cancel
};

/** Asks whether unsaved changes should be saved. */
Answer AskSaveChanges( const std::string& documentName );
/** Asks a yes/no question; returns true for yes. */
bool AskYesNo( const std::string& message );
/** Shows an error message box. */
void ShowError( const std::string& message );
/** Shows a warning message box. */
void ShowWarning( const std::string& message );

}

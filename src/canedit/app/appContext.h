// Copyright © 2026 CCP ehf.

#pragma once

#include "assetLoader.h"
#include "document.h"
#include "pathResolver.h"
#include "session.h"
#include "settings.h"

#include <imgui.h>

#include <string>

namespace canedit
{

class PreviewController; // NOLINT(readability-identifier-naming): forward declaration of a CamelCase class

/** Drag and drop payload carrying a node type (uint32_t) from the palette. */
constexpr const char* NODE_TYPE_PAYLOAD = "CANEDIT_NODE_TYPE";
/** Drag and drop payload carrying a parameter index (uint32_t) from the parameters panel. */
constexpr const char* PARAMETER_PAYLOAD = "CANEDIT_PARAMETER";

/** Objects shared by the parts of the editor UI. */
struct AppContext
{
	PathResolver* resolver = nullptr;
	FileAssetLoader* loader = nullptr;
	Document* document = nullptr;
	EditorSession* session = nullptr;
	EditorSettings* settings = nullptr;
	PreviewController* preview = nullptr;

	/** Shows a message in the status bar. */
	void SetStatus( std::string message, bool error = false )
	{
		status = std::move( message );
		statusIsError = error;
		statusTime = ImGui::GetTime();
	}

	std::string status;
	bool statusIsError = false;
	double statusTime = 0.0;
};

/** Turns activation of the last ImGui item into an edit of the session, so that changing a value by dragging or
* typing is a single undo step. */
inline void TrackEdit( EditorSession& session )
{
	if( ImGui::IsItemActivated() )
	{
		session.BeginEdit();
	}
	if( ImGui::IsItemDeactivated() )
	{
		session.EndEdit();
	}
}

/** Converts between graph vectors and ImGui vectors. */
inline ImVec2 ToImVec2( Vec2 v )
{
	return { v.x, v.y };
}

inline Vec2 ToVec2( ImVec2 v )
{
	return { v.x, v.y };
}

}

// Copyright © 2026 CCP ehf.

#pragma once

#include "../../appState.h"

namespace UiSettings
{
/// call after ImGui::CreateContext() and before the fonts are built
void Initialize( AppState& appState );
void MarkDirty();
void Save();
const std::string& GetIniFilePath();
}

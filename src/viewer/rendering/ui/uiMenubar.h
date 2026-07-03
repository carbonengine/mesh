// Copyright © 2026 CCP ehf.

#pragma once

#include "../../appState.h"

struct MenuState
{
	bool showUi{ true };
};

class UiMenubar
{
public:
	void Render( AppState& appState, MenuState& menuState );
};
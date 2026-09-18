// Copyright © 2026 CCP ehf.

#pragma once

#include "../../appState.h"

class UiMenubar
{
public:
	void Render( AppState& appState );

private:
	void RenderUiScaleMenu( AppState& appState );

	float m_uiScaleSliderValue{ 1.0f };
	bool m_uiScaleSliderActive{ false };
};
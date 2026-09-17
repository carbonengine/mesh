// Copyright © 2026 CCP ehf.

#pragma once
#include <imgui.h>

namespace UiConsts
{
const float MIN_UI_SCALE = 1.0f;
const float MAX_UI_SCALE = 3.0f;

// sizes at 1x, use the scaled accessors below
const float BASE_FONT_SIZE = 13.0f;
const float BASE_FONT_AWESOME_SIZE = 13.0f;
const float BASE_MENU_BAR_HEIGHT = 18.0f;
const float BASE_ANIMATION_PLAYER_HEIGHT = 36.0f;
const float BASE_BUTTON_WIDTH = 19.0f;
const float BASE_BUTTON_HEIGHT = 19.0f;

// the applied scale is whatever the font atlas was built with, so it is derived from the current font rather than stored
inline float Scaled( float baseSize )
{
	return baseSize * ImGui::GetFontSize() / BASE_FONT_SIZE;
}

inline float FontAwesomeSize()
{
	return Scaled( BASE_FONT_AWESOME_SIZE );
}
inline float MenuBarHeight()
{
	return Scaled( BASE_MENU_BAR_HEIGHT );
}
inline float AnimationPlayerHeight()
{
	return Scaled( BASE_ANIMATION_PLAYER_HEIGHT );
}
inline float ButtonWidth()
{
	return Scaled( BASE_BUTTON_WIDTH );
}
inline float ButtonHeight()
{
	return Scaled( BASE_BUTTON_HEIGHT );
}
inline ImVec2 ButtonSize()
{
	return ImVec2( ButtonWidth(), ButtonHeight() );
}
}

// Copyright © 2026 CCP ehf.

#include "uiConsts.h"

namespace UiConsts
{
namespace
{
float g_uiScale = 1.0f;
}

float GetUiScale()
{
	return g_uiScale;
}

void SetUiScale( float scale )
{
	g_uiScale = scale;
}

float Scaled( float baseSize )
{
	return baseSize * g_uiScale;
}
}

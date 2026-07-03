// Copyright © 2026 CCP ehf.

#pragma once

#include "../../appState.h"



class UIAnimationPlayback
{
public:
	void Render( AppState& appState );

private:
	void UpdateState( AppState& appState );

	float m_duration{ 0.0f };
	float m_currentTime{ 0.0f };
	bool m_playing{ false };
	bool m_repeat{ false };
};
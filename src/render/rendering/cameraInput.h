// Copyright © 2026 CCP ehf.

#pragma once

#include <cstdint>

#include <Vector2.h>

/// Mouse input that drives the camera.
struct MouseState
{
	Vector2 position = { 0.0f, 0.0f };
	Vector2 previousPosition = { 0.0f, 0.0f };
	// vertical wheel delta
	float wheelDelta{ 0.0f };
	bool leftButton{ false };
	bool middleButton{ false };
	bool rightButton{ false };

	bool operator==( const MouseState& other ) const
	{
		return position == other.position &&
			previousPosition == other.previousPosition &&
			wheelDelta == other.wheelDelta &&
			leftButton == other.leftButton &&
			middleButton == other.middleButton &&
			rightButton == other.rightButton;
	}
};

/// One-shot camera commands.
enum class CameraTrigger : uint8_t
{
	None,
	Focus,
	LookUp,
	LookDown,
	LookRight,
	LookLeft,
	LookFront,
	LookBack,
};

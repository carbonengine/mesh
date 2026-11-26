// Copyright © 2026 CCP ehf.

#pragma once

/// Skeleton debug visualization options.
struct DebugDrawSettings
{
	bool bones{ false };
	bool joints{ false };
	bool jointAxes{ false };
	/// Size reference for joints and axes, typically the bounding sphere radius of the model
	float radius{ 1.0f };
};

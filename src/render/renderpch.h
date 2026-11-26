// Copyright © 2026 CCP ehf.

// Common includes for the render library. Library headers rely on these, so tools using the library should
// include this header (e.g. from their own precompiled header).

#pragma once

#include <algorithm>
#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <set>
#include <vector>

#include <CcpMath.h>
#include <cmf/cmf.h>
#include <cmf/animation.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include "log.h"

#define PI 3.14159265358979323846f // pi
#define TWO_PI 2.0f * PI

#if !defined( NDEBUG )
#define DEBUG_MODE
#endif
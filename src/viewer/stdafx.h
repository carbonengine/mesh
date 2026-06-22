// Copyright © 2025 CCP ehf.

#pragma once

#include <algorithm>
#include <array>
#include <set>
#include <vector>

#include <CcpMath.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include "log.h"

#define PI 3.14159265358979323846f // pi
#define TWO_PI 2.0f * PI

#if !defined( NDEBUG )
#define DEBUG_MODE
#endif
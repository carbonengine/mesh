// Copyright © 2026 CCP ehf.

#pragma once

#include "../renderable/model.h"
#include "../vulkan/commandbuffer.h"
#include "appState.h"
#include "../renderable/primitive.h"

namespace BoundingBox
{
PrimitiveRenderable Create( std::shared_ptr<const Renderer> renderer, Vector3 color );
};
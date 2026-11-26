// Copyright © 2026 CCP ehf.

#pragma once

#include "../vulkan/commandbuffer.h"
#include "../renderable/primitive.h"

namespace BoundingBox
{
PrimitiveRenderable Create( std::shared_ptr<const Renderer> renderer, Vector3 color );
};
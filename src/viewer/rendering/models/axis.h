#pragma once

#include "../renderable/model.h"
#include "../vulkan/commandbuffer.h"
#include "appState.h"
#include "../renderable/primitive.h"

namespace Axis
{
PrimitiveRenderable Create( std::shared_ptr<const Renderer> renderer );
};
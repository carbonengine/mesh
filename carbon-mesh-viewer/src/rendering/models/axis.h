#pragma once

#include "../renderable/model.h"
#include "../vulkan/commandbuffer.h"
#include "appState.h"

namespace Axis
{
ModelRenderable Create( std::shared_ptr<const Renderer> renderer );
};
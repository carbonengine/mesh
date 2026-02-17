#pragma once

#include "../renderable/model.h"
#include "../vulkan/commandbuffer.h"
#include "appState.h"
#include "../renderable/primitive.h"

namespace BoundingBox
{
struct VertexUBO
{
	Matrix proj;
	Matrix view;
	Matrix model;
};

PrimitiveRenderable Create( std::shared_ptr<const Renderer> renderer, Vector3 color );
};
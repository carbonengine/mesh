// Copyright © 2026 CCP ehf.

#pragma once

#include "../vulkan/graphicseffect.h"

namespace PrimitiveEffects
{

struct VertexUBO
{
	Matrix proj;
	Matrix view;
	Matrix model;
	Vector4 boneInfo; // x = boneCount, y = how many bones transforms are used per instance
};

struct ColorInfo
{
	Vector4 unselected;
	Vector4 selected;
};

GraphicsEffect CreateFlatColorEffect( std::shared_ptr<const Renderer> renderer, ColorInfo colorInfo, GraphicsEffect::Config config, std::vector<uint32_t> vertexToBoneMapping );
GraphicsEffect CreateAxisEffect( std::shared_ptr<const Renderer> renderer );
};
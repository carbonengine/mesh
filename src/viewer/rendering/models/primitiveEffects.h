// Copyright © 2026 CCP ehf.

#pragma once

#include "../vulkan/graphicseffect.h"
#include "cmf/cmf.h"

namespace PrimitiveEffects
{

struct VertexUBO
{
	Matrix proj;
	Matrix view;
	Matrix model;
	Vector4 boneInfo; // x = boneCount, y = how many bones transforms are used per instance
};

struct AxisConfig
{
	Vector3 color;
	float scale;
	uint32_t axisIndex; // 0 for normal, 1 for tangent, 2 for bitangent
};

struct ColorInfo
{
	Vector4 unselected;
	Vector4 selected;
};

GraphicsEffect CreateFlatColorEffect( std::shared_ptr<const Renderer> renderer, ColorInfo colorInfo, GraphicsEffectTypes::Config config, std::vector<uint32_t> vertexToBoneMapping );
GraphicsEffect CreateOrientationEffect( std::shared_ptr<const Renderer> renderer );
GraphicsEffect CreateUnpackedAxisEffect( std::shared_ptr<const Renderer> renderer, const cmf::Usage usage, uint32_t usageIndex, const cmf::Mesh& mesh );
GraphicsEffect CreatePackedAxisEffect( std::shared_ptr<const Renderer> renderer, const cmf::Usage usage, uint32_t usageIndex, const cmf::Mesh& mesh );
GraphicsEffect CreatePackedLegacyAxisEffect( std::shared_ptr<const Renderer> renderer, const cmf::Usage usage, uint32_t usageIndex, const cmf::Mesh& mesh );

};
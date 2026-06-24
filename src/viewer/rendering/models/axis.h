// Copyright © 2026 CCP ehf.

#pragma once

#include "../renderable/model.h"
#include "../vulkan/commandbuffer.h"
#include "appState.h"
#include "../renderable/primitive.h"

namespace Axis
{

PrimitiveRenderable CreateOrientationPrimitive( std::shared_ptr<const Renderer> renderer );

PrimitiveRenderable CreateNormal( std::shared_ptr<const Renderer> renderer, const cmf::Mesh& mesh, const uint32_t usageIndex );
PrimitiveRenderable CreateTangent( std::shared_ptr<const Renderer> renderer, const cmf::Mesh& mesh, const uint32_t usageIndex );
PrimitiveRenderable CreateBinormal( std::shared_ptr<const Renderer> renderer, const cmf::Mesh& mesh, const uint32_t usageIndex );
PrimitiveRenderable CreatePackedNormal( std::shared_ptr<const Renderer> renderer, const cmf::Mesh& mesh, const uint32_t usageIndex );
PrimitiveRenderable CreatePackedTangent( std::shared_ptr<const Renderer> renderer, const cmf::Mesh& mesh, const uint32_t usageIndex );
PrimitiveRenderable CreatePackedBinormal( std::shared_ptr<const Renderer> renderer, const cmf::Mesh& mesh, const uint32_t usageIndex );
PrimitiveRenderable CreatePackedLegacyNormal( std::shared_ptr<const Renderer> renderer, const cmf::Mesh& mesh, const uint32_t usageIndex );
PrimitiveRenderable CreatePackedLegacyTangent( std::shared_ptr<const Renderer> renderer, const cmf::Mesh& mesh, const uint32_t usageIndex );
PrimitiveRenderable CreatePackedLegacyBinormal( std::shared_ptr<const Renderer> renderer, const cmf::Mesh& mesh, const uint32_t usageIndex );
};
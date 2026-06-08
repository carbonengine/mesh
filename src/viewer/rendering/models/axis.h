#pragma once

#include "../renderable/model.h"
#include "../vulkan/commandbuffer.h"
#include "appState.h"
#include "../renderable/primitive.h"

namespace Axis
{

PrimitiveRenderable CreateOrientationPrimitive( std::shared_ptr<const Renderer> renderer );

PrimitiveRenderable CreateNormal( std::shared_ptr<const Renderer> renderer, const cmf::Mesh& mesh );
PrimitiveRenderable CreateTangent( std::shared_ptr<const Renderer> renderer, const cmf::Mesh& mesh );
PrimitiveRenderable CreateBinormal( std::shared_ptr<const Renderer> renderer, const cmf::Mesh& mesh );
PrimitiveRenderable CreatePackedNormal( std::shared_ptr<const Renderer> renderer, const cmf::Mesh& mesh );
PrimitiveRenderable CreatePackedTangent( std::shared_ptr<const Renderer> renderer, const cmf::Mesh& mesh );
PrimitiveRenderable CreatePackedBinormal( std::shared_ptr<const Renderer> renderer, const cmf::Mesh& mesh );
PrimitiveRenderable CreatePackedLegacyNormal( std::shared_ptr<const Renderer> renderer, const cmf::Mesh& mesh );
PrimitiveRenderable CreatePackedLegacyTangent( std::shared_ptr<const Renderer> renderer, const cmf::Mesh& mesh );
PrimitiveRenderable CreatePackedLegacyBinormal( std::shared_ptr<const Renderer> renderer, const cmf::Mesh& mesh );
};
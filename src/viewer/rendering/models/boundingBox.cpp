// Copyright © 2026 CCP ehf.

#include "boundingBox.h"
#include "primitiveEffects.h"

namespace BoundingBox
{

const std::array<Vector4, 8> BOX_VERTICES = {
	Vector4{ -0.5f, -0.5f, -0.5f, 0.0f },
	Vector4{ 0.5f, -0.5f, -0.5f, 0.0f },
	Vector4{ -0.5f, 0.5f, -0.5f, 0.0f },
	Vector4{ 0.5f, 0.5f, -0.5f, 0.0f },
	Vector4{ -0.5f, -0.5f, 0.5f, 0.0f },
	Vector4{ 0.5f, -0.5f, 0.5f, 0.0f },
	Vector4{ -0.5f, 0.5f, 0.5f, 0.0f },
	Vector4{ 0.5f, 0.5f, 0.5f, 0.0f },
};

const std::array<uint32_t, 24> BOX_INDICES = { 0, 1, 0, 2, 0, 4, 3, 1, 3, 2, 3, 7, 5, 1, 5, 7, 5, 4, 6, 4, 6, 2, 6, 7 };

PrimitiveRenderable Create( std::shared_ptr<const Renderer> renderer, Vector3 color )
{
	GraphicsEffectTypes::Config config{};
	config.lineWidth = 2.0f;
	config.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

	auto effect = PrimitiveEffects::CreateFlatColorEffect( renderer, { Vector4( color, 0.0f ), Vector4( color, 0.0f ) }, config, {} );
	auto model = PrimitiveRenderable( renderer, std::move( effect ) );
	model.SetBufferData( reinterpret_cast<const uint8_t*>( BOX_VERTICES.data() ), (uint32_t)BOX_VERTICES.size() * sizeof( Vector4 ), sizeof( Vector4 ) );
	model.SetIndexData( reinterpret_cast<const uint8_t*>( BOX_INDICES.data() ), (uint32_t)BOX_INDICES.size() * sizeof( uint32_t ), sizeof( uint32_t ) );

	return model;
}
}

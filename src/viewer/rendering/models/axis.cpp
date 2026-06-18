// Copyright © 2026 CCP ehf.

#include "axis.h"
#include "primitiveEffects.h"

namespace Axis
{
struct AxisVertex
{
	Vector3 position;
	Vector3 color;
};

const Vector3 RED{ 1.0f, 0.0f, 0.0f };
const Vector3 GREEN{ 0.0f, 1.0f, 0.0f };
const Vector3 BLUE{ 0.0f, 0.0f, 1.0f };

const std::array<AxisVertex, 6> AXIS_MESH = {
	AxisVertex{ { 0.0f, 0.0f, 0.0f }, RED },
	AxisVertex{ { 1.0f, 0.0f, 0.0f }, RED },
	AxisVertex{ { 0.0f, 0.0f, 0.0f }, GREEN },
	AxisVertex{ { 0.0f, 1.0f, 0.0f }, GREEN },
	AxisVertex{ { 0.0f, 0.0f, 0.0f }, BLUE },
	AxisVertex{ { 0.0f, 0.0f, 1.0f }, BLUE },
};

PrimitiveRenderable Create( std::shared_ptr<const Renderer> renderer )
{
	auto effect = PrimitiveEffects::CreateAxisEffect( renderer );

	auto model = PrimitiveRenderable( renderer, std::move( effect ) );
	model.SetBufferData( reinterpret_cast<const uint8_t*>( AXIS_MESH.data() ), (uint32_t)AXIS_MESH.size() * sizeof( AxisVertex ), sizeof( AxisVertex ) );

	return model;
}

}

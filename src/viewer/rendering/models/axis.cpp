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

PrimitiveRenderable CreateOrientationPrimitive( std::shared_ptr<const Renderer> renderer )
{
	auto effect = PrimitiveEffects::CreateOrientationEffect( renderer );

	auto model = PrimitiveRenderable( renderer, std::move( effect ) );
	model.SetBufferData( reinterpret_cast<const uint8_t*>( AXIS_MESH.data() ), (uint32_t)AXIS_MESH.size() * sizeof( AxisVertex ), sizeof( AxisVertex ) );

	return model;
}

PrimitiveRenderable CreateNormal( std::shared_ptr<const Renderer> renderer, const cmf::Mesh& mesh )
{
	// the bufferdata is the output of the geoprepass and will be handled later
	return PrimitiveRenderable( renderer, PrimitiveEffects::CreateUnpackedAxisEffect( renderer, cmf::Usage::Normal, mesh ) );
}

PrimitiveRenderable CreateTangent( std::shared_ptr<const Renderer> renderer, const cmf::Mesh& mesh )
{
	// the bufferdata is the output of the geoprepass and will be handled later
	return PrimitiveRenderable( renderer, PrimitiveEffects::CreateUnpackedAxisEffect( renderer, cmf::Usage::Tangent, mesh ) );
}

PrimitiveRenderable CreateBinormal( std::shared_ptr<const Renderer> renderer, const cmf::Mesh& mesh )
{
	// the bufferdata is the output of the geoprepass and will be handled later
	return PrimitiveRenderable( renderer, PrimitiveEffects::CreateUnpackedAxisEffect( renderer, cmf::Usage::Binormal, mesh ) );
}

PrimitiveRenderable CreatePackedNormal( std::shared_ptr<const Renderer> renderer, const cmf::Mesh& mesh )
{
	// the bufferdata is the output of the geoprepass and will be handled later
	return PrimitiveRenderable( renderer, PrimitiveEffects::CreatePackedAxisEffect( renderer, cmf::Usage::Normal, mesh ) );
}

PrimitiveRenderable CreatePackedTangent( std::shared_ptr<const Renderer> renderer, const cmf::Mesh& mesh )
{
	// the bufferdata is the output of the geoprepass and will be handled later
	return PrimitiveRenderable( renderer, PrimitiveEffects::CreatePackedAxisEffect( renderer, cmf::Usage::Tangent, mesh ) );
}

PrimitiveRenderable CreatePackedBinormal( std::shared_ptr<const Renderer> renderer, const cmf::Mesh& mesh )
{
	// the bufferdata is the output of the geoprepass and will be handled later
	return PrimitiveRenderable( renderer, PrimitiveEffects::CreatePackedAxisEffect( renderer, cmf::Usage::Binormal, mesh ) );
}

PrimitiveRenderable CreatePackedLegacyNormal( std::shared_ptr<const Renderer> renderer, const cmf::Mesh& mesh )
{
	// the bufferdata is the output of the geoprepass and will be handled later
	return PrimitiveRenderable( renderer, PrimitiveEffects::CreatePackedLegacyAxisEffect( renderer, cmf::Usage::Normal, mesh ) );
}

PrimitiveRenderable CreatePackedLegacyTangent( std::shared_ptr<const Renderer> renderer, const cmf::Mesh& mesh )
{
	// the bufferdata is the output of the geoprepass and will be handled later
	return PrimitiveRenderable( renderer, PrimitiveEffects::CreatePackedLegacyAxisEffect( renderer, cmf::Usage::Tangent, mesh ) );
}

PrimitiveRenderable CreatePackedLegacyBinormal( std::shared_ptr<const Renderer> renderer, const cmf::Mesh& mesh )
{
	// the bufferdata is the output of the geoprepass and will be handled later
	return PrimitiveRenderable( renderer, PrimitiveEffects::CreatePackedLegacyAxisEffect( renderer, cmf::Usage::Binormal, mesh ) );
}

}

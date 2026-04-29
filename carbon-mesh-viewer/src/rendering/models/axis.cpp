#include "axis.h"
#include "../vulkan/effect.h"

namespace Axis
{
struct AxisVertex
{
	float position[3];
	float color[3];
};

const AxisVertex AXIS_MESH[6] = {
	{ { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
	{ { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
	{ { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
	{ { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
	{ { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
	{ { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
};

PrimitiveRenderable Create( std::shared_ptr<const Renderer> renderer )
{
	GraphicsEffect::Config config{};
	config.availableVertexElements = {
		{ cmf::Usage::Position,
		  0,
		  cmf::ElementType::Float32,
		  3,
		  0 },
		{ cmf::Usage::Color,
		  0,
		  cmf::ElementType::Float32,
		  3,
		  sizeof( Vector3 ) },

	};
	config.lineWidth = 2.0f;
	config.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	config.stride = sizeof( AxisVertex );

	auto effect = GraphicsEffect( renderer );
	effect.RegisterUniformData( VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, 0, nullptr, sizeof( Matrix ) * 2 );
	effect.SetConfig( config );
	effect.SetShaderName( "orientationgizmo" );

	auto model = PrimitiveRenderable( renderer, std::move( effect ) );
	model.SetBufferData( reinterpret_cast<const uint8_t*>( AXIS_MESH ), sizeof( AXIS_MESH ), sizeof( AxisVertex ) );

	return model;
}
}

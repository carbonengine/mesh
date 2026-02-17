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
	Effect::Config config{};
	config.vertexDescriptions = {
		{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 }, // position
		{ 1, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof( Vector3 ) }, // color
	};
	config.lineWidth = 2.0f;
	config.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	config.vertexStride = sizeof( AxisVertex );

	auto effect = Effect( renderer );
	effect.RegisterUniformData( VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, 0, nullptr, sizeof( Matrix ) * 2 );
	effect.SetConfig( config );
	effect.SetShaderName( "orientationgizmo" );

	auto model = PrimitiveRenderable( renderer, effect );
	model.SetBufferData( reinterpret_cast<const uint8_t*>( AXIS_MESH ), sizeof( AXIS_MESH ), sizeof( AxisVertex ) );

	return model;
}
}

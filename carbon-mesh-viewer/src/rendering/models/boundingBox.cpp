#include "boundingBox.h"

namespace BoundingBox
{
struct BoxVertex
{
	float position[3];
};

const BoxVertex BOX_VERTICES[8] = {
	{ -0.5f, -0.5f, -0.5f },
	{ 0.5f, -0.5f, -0.5f },
	{ -0.5f, 0.5f, -0.5f },
	{ 0.5f, 0.5f, -0.5f },
	{ -0.5f, -0.5f, 0.5f },
	{ 0.5f, -0.5f, 0.5f },
	{ -0.5f, 0.5f, 0.5f },
	{ 0.5f, 0.5f, 0.5f },
};

const uint32_t BOX_INDICES[24] = { 0, 1, 0, 2, 0, 4, 3, 1, 3, 2, 3, 7, 5, 1, 5, 7, 5, 4, 6, 4, 6, 2, 6, 7 };

PrimitiveRenderable Create( std::shared_ptr<const Renderer> renderer, Vector3 color )
{
	GraphicsEffect::Config config{};
	config.availableVertexElements = {
		{ cmf::Usage::Position,
		  0,
		  cmf::ElementType::Float32,
		  3,
		  0 }
	};
	config.lineWidth = 2.0f;
	config.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	config.stride = sizeof( BoxVertex );

	auto effect = GraphicsEffect( renderer );
	effect.RegisterUniformData<VertexUBO>( VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, 0 );
	effect.RegisterUniformData<Vector3>( VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT, 1, color );
	effect.SetConfig( config );
	effect.SetShaderName( "flatcolor" );

	auto model = PrimitiveRenderable( renderer, std::move( effect ) );
	model.SetBufferData( reinterpret_cast<const uint8_t*>( BOX_VERTICES ), sizeof( BOX_VERTICES ), sizeof( BoxVertex ) );
	model.SetIndexData( reinterpret_cast<const uint8_t*>( BOX_INDICES ), sizeof( BOX_INDICES ), sizeof( uint32_t ) );

	return model;
}
}

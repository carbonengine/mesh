#include "axis.h"

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
	auto model = PrimitiveRenderable( renderer );

    model.SetBufferData( reinterpret_cast<const uint8_t*>( AXIS_MESH ), sizeof( AXIS_MESH ), sizeof( AxisVertex ) );
    model.SetVertexDescriptions( {
        { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 }, // position
        { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, 3 * sizeof( float ) }, // color
	} );
	model.SetLineWidth( 2.0f );
	model.SetTopology( VK_PRIMITIVE_TOPOLOGY_LINE_LIST );
	return model;
}
}

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

ModelRenderable Create( std::shared_ptr<const Renderer> renderer )
{
	auto model = ModelRenderable( renderer );
	auto mesh = MeshRenderable( renderer );
	auto lod = MeshLodRenderable( renderer );

	lod.SetVertexData( reinterpret_cast<const uint8_t*>( AXIS_MESH ), sizeof( AXIS_MESH ), sizeof( AxisVertex ) );
	lod.AddArea( 0, 6 );

	mesh.AddLodRenderable( std::move( lod ) );
	mesh.SetVertexDescriptions( {
		{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 }, // position
		{ 1, 0, VK_FORMAT_R32G32B32_SFLOAT, 3 * sizeof( float ) }, // color
	} );
	mesh.SetStride( sizeof( AxisVertex ) );
	mesh.SetTopology( VK_PRIMITIVE_TOPOLOGY_LINE_LIST );
	mesh.SetLineWidth( 2.0f );
	model.AddMeshRenderable( std::move( mesh ) );

	return model;
}
}

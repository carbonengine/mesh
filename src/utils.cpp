#include "cmf/utils.h"

namespace cmf
{

CcpMath::AxisAlignedBox CalculateBounds( const Mesh& mesh, const void* vb )
{
	CcpMath::AxisAlignedBox bounds;
	auto element = std::find_if( mesh.decl.begin(), mesh.decl.end(), []( const auto& x ) { return x.usage == Usage::Position && x.usageIndex == 0; } );
	if( element == mesh.decl.end() )
	{
		return bounds;
	}
    for( auto pos : BufferElementStream<Vector3>( *element, vb, mesh.vb.size / mesh.vb.stride, mesh.vb.stride ) )
    {
		bounds.Include( pos );
    }
	return bounds;
}

CcpMath::AxisAlignedBox CalculateBounds( const Mesh& mesh, const void* vb, const void* ib, uint32_t firstElement, uint32_t elementCount )
{
	return CalculateBounds( mesh, vb );
}

//CcpMath::AxisAlignedBox CalculateBounds( const Mesh& mesh, uint32_t firstElement, uint32_t elementCount );

}
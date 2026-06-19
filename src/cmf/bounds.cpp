// Copyright © 2025 CCP ehf.

#include "cmf/bounds.h"
#include "cmf/bufferstreams.h"

namespace cmf
{

CcpMath::AxisAlignedBox CalculateBounds( const Mesh& mesh, const BufferManager& buffers )
{
	CcpMath::AxisAlignedBox bounds;
	auto element = std::find_if( mesh.decl.begin(), mesh.decl.end(), []( const auto& x ) { return x.usage == Usage::Position && x.usageIndex == 0; } );
	if( element == mesh.decl.end() )
	{
		return bounds;
	}
	for( auto& lod : mesh.lods )
	{
		for( auto pos : BufferElementStream<Vector3>( *element, lod.vb, buffers ) )
		{
			bounds.Include( pos );
		}
		break;
	}
	return bounds;
}

CcpMath::AxisAlignedBox CalculateAreaBounds( const Mesh& mesh, uint32_t areaIndex, const BufferManager& buffers )
{
	CcpMath::AxisAlignedBox bounds;
	auto element = std::find_if( mesh.decl.begin(), mesh.decl.end(), []( const auto& x ) { return x.usage == Usage::Position && x.usageIndex == 0; } );
	if( element == mesh.decl.end() )
	{
		return bounds;
	}
	for( auto& lod : mesh.lods )
	{
		auto indices = IndexBufferStream( lod.ib, buffers );
		auto positions = BufferElementStream<Vector3>( *element, lod.vb, buffers );
		auto first = lod.areas[areaIndex].firstElement * 3;
		for( uint32_t i = 0; i < lod.areas[areaIndex].elementCount * 3; ++i )
		{
			bounds.Include( positions[indices[first + i]] );
		}
		break;
	}
	return bounds;
}

}

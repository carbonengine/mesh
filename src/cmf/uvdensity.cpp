// Copyright © 2025 CCP ehf.

#include "cmf/uvdensity.h"
#include "cmf/bufferstreams.h"

namespace
{

float GetMeshDiameter( const cmf::BufferElementStream<Vector3>& posStream )
{
	CcpMath::AxisAlignedBox bounds;
	for( auto pos : posStream )
	{
		bounds.Include( pos );
	}
	return CcpMath::Sphere( bounds ).radius * 2;
}

}

namespace cmf
{

float CalculateUvDensity( const cmf::Mesh& mesh, const cmf::VertexElement& posDecl, const cmf::VertexElement& uvDecl, const cmf::BufferManager& buffers )
{
	const cmf::BufferElementStream<Vector3> posStream( posDecl, mesh.lods[0].vb, buffers );
	const cmf::BufferElementStream<Vector4> uvStream( uvDecl, mesh.lods[0].vb, buffers );
	const cmf::IndexBufferStream indexStream( mesh.lods[0].ib, buffers );

	auto diameter = GetMeshDiameter( posStream );
	double totalArea = 0;
	std::vector<std::pair<float, float>> densities;

	for( uint32_t i = 0; i < indexStream.size(); i += 3 )
	{
		Vector3 verts[3] = { posStream[indexStream[i]], posStream[indexStream[i + 1]], posStream[indexStream[i + 2]] };
		Vector4 uvs[3] = { uvStream[indexStream[i]], uvStream[indexStream[i + 1]], uvStream[indexStream[i + 2]] };

		float density = 0;
		float edges[3];
		bool valid = true;
		for( uint32_t i = 0; i < 3; ++i )
		{
			auto dx = Length( verts[i] - verts[( i + 1 ) % 3] );
			if( dx == 0 )
			{
				valid = false;
				break;
			}
			edges[i] = dx;
			auto dy = 0.f;
			for( int32_t k = 0; k < 4; ++k )
			{
				dy += ( uvs[i][k] - uvs[( i + 1 ) % 3][k] ) * ( uvs[i][k] - uvs[( i + 1 ) % 3][k] );
			}
			if( dy != 0.f )
			{
				dy = sqrt( dy ) * diameter;
				if( i == 0 )
				{
					density = dy / dx;
				}
				else
				{
					density = std::min( density, dy / dx );
				}
			}
		}
		if( valid )
		{
			double p = 0.5f * ( edges[0] + edges[1] + edges[2] );
			auto area = sqrt( std::max( p * ( p - edges[0] ) * ( p - edges[1] ) * ( p - edges[2] ), 0. ) );
			totalArea += area;
			densities.push_back( { float( area ), density } );
		}
	}
	const float discardArea = 0.03f;

	if( !densities.empty() )
	{
		std::sort( begin( densities ), end( densities ), []( auto x, auto y ) { return x.second < y.second; } );
		size_t offset = 0;
		float a = 0;
		while( a < totalArea * discardArea )
		{
			a += densities[offset].first;
			++offset;
		}
		return densities[offset].second;
	}
	return 0;
}

std::vector<float> CalculateUvDensities( const cmf::Mesh& mesh, const cmf::BufferManager& buffers )
{
	const cmf::VertexElement* posDecl = nullptr;
	uint32_t uvSetCount = 0;
	for( auto& decl : mesh.decl )
	{
		if( decl.usage == cmf::Usage::TexCoord )
		{
			uvSetCount = std::max( uvSetCount, uint32_t( decl.usageIndex + 1 ) );
		}
		else if( decl.usage == cmf::Usage::Position && decl.usageIndex == 0 )
		{
			posDecl = &decl;
		}
	}
	if( uvSetCount == 0 || posDecl == nullptr )
	{
		return {};
	}
	std::vector<float> densities( uvSetCount, 0.f );
	for( auto& decl : mesh.decl )
	{
		if( decl.usage == cmf::Usage::TexCoord )
		{
			densities[decl.usageIndex] = CalculateUvDensity( mesh, *posDecl, decl, buffers );
		}
	}
	return densities;
}

}
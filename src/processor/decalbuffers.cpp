// Copyright © 2026 CCP ehf.

#include "commands.h"
#include "cmffile.h"
#include "cmf/bufferstreams.h"
#include "cmf/declutils.h"
#include <nlohmann/json.hpp>


namespace
{

struct GenerateDecalBufferArgs
{
	std::string path;
	uint32_t meshIndex = 0;
	std::vector<std::string> decals;
};


bool AxisTest( float a, float b, float fa, float fb, const Vector3& v0, const Vector3& v1, int axis0, int axis1 )
{
	float p0 = a * v0[axis0] + b * v0[axis1];
	float p2 = a * v1[axis0] + b * v1[axis1];
	if( p2 < p0 )
	{
		std::swap( p0, p2 );
	}
	const float rad = fa + fb;
	return p0 <= rad && p2 >= -rad;
}

bool IntersectTriangleOrientedBox( const Vector3& v0,
								   const Vector3& v1,
								   const Vector3& v2,
								   const Matrix& invOrientedBox )
{
	// put triangle points in "inverse bounding box" space
	std::array<Vector3, 3> v = {
		TransformCoord( v0, invOrientedBox ),
		TransformCoord( v1, invOrientedBox ),
		TransformCoord( v2, invOrientedBox )
	};

	// checks box sides as separating plane
	if( ( v[0].x < -1.f ) && ( v[1].x < -1.f ) && ( v[2].x < -1.f ) )
	{
		return false;
	}
	if( ( v[0].x > 1.f ) && ( v[1].x > 1.f ) && ( v[2].x > 1.f ) )
	{
		return false;
	}
	if( ( v[0].y < -1.f ) && ( v[1].y < -1.f ) && ( v[2].y < -1.f ) )
	{
		return false;
	}
	if( ( v[0].y > 1.f ) && ( v[1].y > 1.f ) && ( v[2].y > 1.f ) )
	{
		return false;
	}
	if( ( v[0].z < -1.f ) && ( v[1].z < -1.f ) && ( v[2].z < -1.f ) )
	{
		return false;
	}
	if( ( v[0].z > 1.f ) && ( v[1].z > 1.f ) && ( v[2].z > 1.f ) )
	{
		return false;
	}

	const Vector3 e0 = v[1] - v[0];
	const Vector3 e1 = v[2] - v[1];
	const Vector3 e2 = v[0] - v[2];

	// check triangle plane as a separating plane
	Vector3 normal = Cross( e0, e1 );

	Vector3 vmin;
	Vector3 vmax;
	for( int i = 0; i < 3; ++i )
	{
		if( normal[i] > 0 )
		{
			vmin[i] = -1 - v[0][i];
			vmax[i] = 1 - v[0][i];
		}
		else
		{
			vmin[i] = 1 - v[0][i];
			vmax[i] = -1 - v[0][i];
		}
	}
	if( Dot( vmin, normal ) > 0 )
	{
		return false;
	}
	if( Dot( vmax, normal ) < 0 )
	{
		return false;
	}

	// 9 more separating plane checks for cross products between triangle edges
	// and box sides
	float fex = std::abs( e0.x );
	float fey = std::abs( e0.y );
	float fez = std::abs( e0.z );
	if( !AxisTest( e0.z, -e0.y, fez, fey, v[0], v[2], 1, 2 ) )
	{
		return false;
	}
	if( !AxisTest( -e0.z, e0.x, fez, fex, v[0], v[2], 0, 2 ) )
	{
		return false;
	}
	if( !AxisTest( e0.y, -e0.x, fey, fex, v[1], v[2], 0, 1 ) )
	{
		return false;
	}

	fex = std::abs( e1.x );
	fey = std::abs( e1.y );
	fez = std::abs( e1.z );
	if( !AxisTest( e1.z, -e1.y, fez, fey, v[0], v[2], 1, 2 ) )
	{
		return false;
	}
	if( !AxisTest( -e1.z, e1.x, fez, fex, v[0], v[2], 0, 2 ) )
	{
		return false;
	}
	if( !AxisTest( e1.y, -e1.x, fey, fex, v[0], v[1], 0, 1 ) )
	{
		return false;
	}

	fex = std::abs( e2.x );
	fey = std::abs( e2.y );
	fez = std::abs( e2.z );
	if( !AxisTest( e2.z, -e2.y, fez, fey, v[0], v[1], 1, 2 ) )
	{
		return false;
	}
	if( !AxisTest( -e2.z, e2.x, fez, fex, v[0], v[1], 0, 2 ) )
	{
		return false;
	}
	if( !AxisTest( e2.y, -e2.x, fey, fex, v[1], v[2], 0, 1 ) )
	{
		return false;
	}

	return true;
}

Matrix ExtractDecalInvTransform( const char* decalTransform )
{
	const std::string arrayStr = std::string( "[" ) + decalTransform + "]";
	auto decalArray = nlohmann::json::parse( arrayStr );

	const Vector3 position = Vector3( decalArray[0], decalArray[1], decalArray[2] );
	const Quaternion rotation = Quaternion( decalArray[3], decalArray[4], decalArray[5], decalArray[6] );
	const Vector3 scale = Vector3( decalArray[7], decalArray[8], decalArray[9] );

	auto decalMatrix = TransformationMatrix( scale, rotation, position );
	return Inverse( decalMatrix );
}

void GenerateDecalBuffers( CLI::App& app, GenerateDecalBufferArgs& args )
{
	app.add_option( "--decal", args.decals, "Decal transform: position, rotation, scale coordinates, separated with commas" )->required();
	app.add_option( "--mesh", args.meshIndex, "Index of the mesh in the CMF file to generate decal buffers for" );
	app.add_option( "file", args.path, "Path to CMF file to generate decal buffers for" )->required()->check( CLI::ExistingFile );

	app.final_callback( [&args]() {
		CmfFile file{ args.path };
		auto& data = file.GetData();
		auto& bufferManager = file.GetBufferManager();

		if( args.meshIndex >= data.meshes.size() )
		{
			throw std::runtime_error( "Mesh index out of range: " + std::to_string( args.meshIndex ) );
		}
		if( data.meshes[args.meshIndex].topology != cmf::MeshTopology::TriangleList )
		{
			throw std::runtime_error( "Mesh topology must be TriangleList to generate decal buffers" );
		}
		const auto* positionElement = cmf::FindElement( data.meshes[args.meshIndex].decl, cmf::Usage::Position );
		if( !positionElement )
		{
			throw std::runtime_error( "Mesh does not have a position element in its vertex declaration" );
		}

		// decal / lod / indices
		std::vector<std::vector<std::vector<uint32_t>>> decalIndexBuffers;

		for( const auto& decalStr : args.decals )
		{
			auto invDecalMatrix = ExtractDecalInvTransform( decalStr.c_str() );

			auto& decalIBs = decalIndexBuffers.emplace_back();

			for( auto& lod : data.meshes[args.meshIndex].lods )
			{
				auto& decalLodIB = decalIBs.emplace_back();

				const cmf::ConstIndexBufferStream indexStream( lod.ib, bufferManager );
				const cmf::ConstBufferElementStream<Vector3> positionStream( *positionElement, lod.vb, bufferManager );

				for( uint32_t i = 0; i < indexStream.size(); i += 3 )
				{
					const auto v0 = positionStream[indexStream[i]];
					const auto v1 = positionStream[indexStream[i + 1]];
					const auto v2 = positionStream[indexStream[i + 2]];

					if( IntersectTriangleOrientedBox( v0, v1, v2, invDecalMatrix ) )
					{
						decalLodIB.push_back( indexStream[i] );
						decalLodIB.push_back( indexStream[i + 1] );
						decalLodIB.push_back( indexStream[i + 2] );
					}
				}
			}
		}
		printf( "%s\n", nlohmann::json( decalIndexBuffers ).dump().c_str() ); // NOLINT(cppcoreguidelines-pro-type-vararg)
	} );
}
}

REGISTER_COMMAND( "decalbuffers", "Generates decal index buffers for meshes in a CMF file", &GenerateDecalBuffers );

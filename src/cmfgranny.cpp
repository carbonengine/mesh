#include "cmf/cmf.h"
#include "cmf/memallocator.h"
#include "cmf/writer.h"
#include <vector>
#include <map>
#include <unordered_set>
#include <string>
#include <numeric>

#define BUILDING_GRANNY_STATIC 1
#include <granny.h>


using namespace cmf;


struct BoundingBox
{
	granny_real32 min[3];
	granny_real32 max[3];
};

struct AreaBoundsInfo
{
	BoundingBox bounds;
	granny_int32 vertexCount;
};


#ifdef _WIN32
#pragma pack( push, 4 )
#endif

struct MeshBoundsInfo
{
	const char* typeName;
	BoundingBox bounds;
	granny_int32 areaCount;
	AreaBoundsInfo* areaInfos;
	granny_int32 sourceMeshIndex;
	granny_int32 maxScreenSize;
	granny_int32 uvDensityCount;
	granny_real32* uvDensities;
}
#ifndef _WIN32
// On non-windows x64 platforms areaInfos maybe 64bit aligned
__attribute__( ( packed ) )
#endif
;

#ifdef _WIN32
#pragma pack( pop )
#endif


granny_data_type_definition BoundingBoxType[] = {
	{ GrannyReal32Member, "min", 0, 3 },
	{ GrannyReal32Member, "max", 0, 3 },
	{ GrannyEndMember }
};

granny_data_type_definition AreaBoundsInfoType[] = {
	{ GrannyInlineMember, "bounds", BoundingBoxType },
	{ GrannyInt32Member, "vertexCount" },
	{ GrannyEndMember }
};

granny_data_type_definition UvDensityInfoType[] = {
	{ GrannyReal32Member, "density" },
	{ GrannyEndMember }
};

granny_data_type_definition MeshBoundsInfoType[] = {
	{ GrannyStringMember, "typeName" },
	{ GrannyInlineMember, "bounds", BoundingBoxType },
	{ GrannyReferenceToArrayMember, "areaInfo", AreaBoundsInfoType },
	{ GrannyInt32Member, "sourceMeshIndex" },
	{ GrannyInt32Member, "maxScreenSize" },
	{ GrannyReferenceToArrayMember, "uvDensities", UvDensityInfoType },
	{ GrannyEndMember }
};

static const unsigned int grannyTypeSizes[] = {

	0, // 		GrannyEndMember 0 ,
	0, // 		GrannyInlineMember 1,
	0, // 		GrannyReferenceMember  2,
	0, // 		GrannyReferenceToArrayMember 3,
	0, // 		GrannyArrayOfReferencesMember 4,
	0, // 		GrannyVariantReferenceMember 5,
	0, // 		GrannyUnsupportedMemberType_Remove 6,
	0, // 		GrannyReferenceToVariantArrayMember 7,
	0, // 		GrannyStringMember 8 ,
	0, // 		GrannyTransformMembe 9 r,
	4, // 		GrannyReal32Member 10,
	1, // 		GrannyInt8Member 11,
	1, // 		GrannyUInt8Member 12,
	1, // 		GrannyBinormalInt8Member 13,
	1, // 		GrannyNormalUInt8Member  14,
	2, // 		GrannyInt16Member  15,
	2, // 		GrannyUInt16Member 16,
	2, // 		GrannyBinormalInt16Member 17,
	2, // 		GrannyNormalUInt16Member 18,
	4, // 		GrannyInt32Member 19 ,
	4, // 		GrannyUInt32Member 20 ,
	2, // 		GrannyReal16Member 21,
	0, // 		GrannyEmptyReferenceMember, 22
};

unsigned int GetGrannyTypeSize( granny_member_type t )
{
	return grannyTypeSizes[(int)t];
}


VertexElement Convert( const granny_data_type_definition& src, uint32_t& offset )
{
	auto MatchName = []( const char* name, const char* prefix, uint8_t& usageIndex ) -> bool {
		if( !strncmp( name, prefix, strlen( prefix ) ) )
		{
			usageIndex = uint8_t( atoi( name + strlen( prefix ) ) );
			return true;
		}
		return false;
	};
	VertexElement dst;
	if( MatchName( src.Name, GrannyVertexPositionName, dst.usageIndex ) )
	{
		dst.usage = Usage::Position;
	}
	else if( MatchName( src.Name, GrannyVertexDiffuseColorName, dst.usageIndex ) )
	{
		dst.usage = Usage::Color;
	}
	else if( MatchName( src.Name, GrannyVertexNormalName, dst.usageIndex ) )
	{
		dst.usage = Usage::Normal;
	}
	else if( MatchName( src.Name, GrannyVertexTangentName, dst.usageIndex ) )
	{
		dst.usage = Usage::Tangent;
	}
	else if( MatchName( src.Name, GrannyVertexBinormalName, dst.usageIndex ) )
	{
		dst.usage = Usage::Binormal;
	}
	else if( MatchName( src.Name, GrannyVertexTextureCoordinatesName, dst.usageIndex ) )
	{
		dst.usage = Usage::TexCoord;
	}
	else if( MatchName( src.Name, GrannyVertexBoneIndicesName, dst.usageIndex ) )
	{
		dst.usage = Usage::BlendIndices;
	}
	else if( MatchName( src.Name, GrannyVertexBoneWeightsName, dst.usageIndex ) )
	{
		dst.usage = Usage::BlendWeights;
	}
	else
	{
		throw std::runtime_error( "Unknown vertex element" );
	}

	uint32_t size = 0;
	switch( src.Type )
	{
	case GrannyInt8Member:
		dst.type = ElementType::Int8;
		size = 1;
		break;
	case GrannyUInt8Member:
		dst.type = ElementType::UInt8;
		size = 1;
		break;
	case GrannyReal16Member:
		dst.type = ElementType::Float16;
		size = 2;
		break;
	case GrannyNormalUInt8Member:
		dst.type = ElementType::UInt8Norm;
		size = 1;
		break;
	case GrannyReal32Member:
		dst.type = ElementType::Float32;
		size = 4;
		break;
	default:
		throw std::runtime_error( "Unsupported data type" );
	}
	dst.elementCount = src.ArrayWidth;
	dst.offset = offset;
	offset += size * dst.elementCount;
	return dst;
}

granny_data_type_definition Convert( const VertexElement& element, granny_memory_arena* memArena )
{
	granny_data_type_definition dst;

	auto SetName = [&]( const char* prefix ) {
		if( element.usageIndex == 0 )
		{
			dst.Name = prefix;
		}
		else
		{
			auto name = prefix + std::to_string( element.usageIndex );
			auto str = static_cast<char*>( GrannyMemoryArenaPush( memArena, name.size() + 1 ) );
			memcpy( str, name.c_str(), name.size() + 1 );
			dst.Name = str;
		}
	};
	switch( element.usage )
	{
	case Usage::Position:
		SetName( GrannyVertexPositionName );
		break;
	case Usage::Color:
		SetName( GrannyVertexDiffuseColorName );
		break;
	case Usage::Normal:
		SetName( GrannyVertexNormalName );
		break;
	case Usage::Tangent:
		SetName( GrannyVertexTangentName );
		break;
	case Usage::Binormal:
		SetName( GrannyVertexBinormalName );
		break;
	case Usage::TexCoord:
		SetName( GrannyVertexTextureCoordinatesName );
		break;
	case Usage::BlendIndices:
		SetName( GrannyVertexBoneIndicesName );
		break;
	case Usage::BlendWeights:
		SetName( GrannyVertexBoneWeightsName );
		break;
	default:
		throw std::runtime_error( "Unknown vertex element" );
	}
	switch( element.type )
	{
	case ElementType::Int8:
		dst.Type = GrannyInt8Member;
		break;
	case ElementType::UInt8:
		dst.Type = GrannyUInt8Member;
		break;
	case ElementType::Float16:
		dst.Type = GrannyReal16Member;
		break;
	case ElementType::UInt8Norm:
		dst.Type = GrannyNormalUInt8Member;
		break;
	case ElementType::Float32:
		dst.Type = GrannyReal32Member;
		break;
	default:
		throw std::runtime_error( "Unsupported data type" );
	}
	dst.ArrayWidth = element.elementCount;
	return dst;
}


template <typename T>
using ConversionFunction = T ( * )( const void* );


template <typename T>
ConversionFunction<T> GetConversionFunction( granny_member_type type )
{
	return []( const void* data ) { return *reinterpret_cast<const T*>( data ); };
}

template <>
ConversionFunction<float> GetConversionFunction( granny_member_type type )
{
	switch( type )
	{
	case GrannyReal32Member:
		return []( const void* data ) { return *reinterpret_cast<const float*>( data ); };
	case GrannyReal16Member:
		return []( const void* data ) {
			float v;
			GrannyReal16ToReal32( *reinterpret_cast<const granny_real16*>( data ), &v );
			return v;
		};
	default:
		throw std::runtime_error( "Unsupported data type" );
	}
}

template <>
ConversionFunction<Vector2> GetConversionFunction( granny_member_type type )
{
	switch( type )
	{
	case GrannyReal32Member:
		return []( const void* data ) { return *reinterpret_cast<const Vector2*>( data ); };
	case GrannyReal16Member:
		return []( const void* data ) {
			Vector2 v;
			GrannyReal16ToReal32( reinterpret_cast<const granny_real16*>( data )[0], &v.x );
			GrannyReal16ToReal32( reinterpret_cast<const granny_real16*>( data )[1], &v.y );
			return v;
		};
	default:
		throw std::runtime_error( "Unsupported data type" );
	}
}

template <>
ConversionFunction<Vector3> GetConversionFunction( granny_member_type type )
{
	switch( type )
	{
	case GrannyReal32Member:
		return []( const void* data ) { return *reinterpret_cast<const Vector3*>( data ); };
	case GrannyReal16Member:
		return []( const void* data ) {
			Vector3 v;
			GrannyReal16ToReal32( reinterpret_cast<const granny_real16*>( data )[0], &v.x );
			GrannyReal16ToReal32( reinterpret_cast<const granny_real16*>( data )[1], &v.y );
			GrannyReal16ToReal32( reinterpret_cast<const granny_real16*>( data )[2], &v.z );
			return v;
		};
	default:
		throw std::runtime_error( "Unsupported data type" );
	}
}

template <>
ConversionFunction<Vector4> GetConversionFunction( granny_member_type type )
{
	switch( type )
	{
	case GrannyReal32Member:
		return []( const void* data ) { return *reinterpret_cast<const Vector4*>( data ); };
	case GrannyReal16Member:
		return []( const void* data ) {
			Vector4 v;
			GrannyReal16ToReal32( reinterpret_cast<const granny_real16*>( data )[0], &v.x );
			GrannyReal16ToReal32( reinterpret_cast<const granny_real16*>( data )[1], &v.y );
			GrannyReal16ToReal32( reinterpret_cast<const granny_real16*>( data )[2], &v.z );
			GrannyReal16ToReal32( reinterpret_cast<const granny_real16*>( data )[3], &v.w );
			return v;
		};
	default:
		throw std::runtime_error( "Unsupported data type" );
	}
}


template <typename T>
class GrannyDataStream
{
public:
	GrannyDataStream( const granny_vertex_data& vertexData, const char* elementName )
	{
		auto vt = vertexData.VertexType;
		uint32_t offset = 0xffffffff;
		uint32_t size = 0;
		for( ; vt->Type != GrannyEndMember; ++vt )
		{
			if( !strcmp( vt->Name, elementName ) )
			{
				offset = size;
				m_conversion = GetConversionFunction<T>( vt->Type );
				m_type = *vt;
				break;
			}
			size += GetGrannyTypeSize( vt->Type ) * vt->ArrayWidth;
		}
		if( offset != 0xffffffff )
		{
			m_data = reinterpret_cast<const uint8_t*>( vertexData.Vertices + offset );
			m_count = vertexData.VertexCount;
			m_stride = GrannyGetTotalObjectSize( vertexData.VertexType );
		}
	}

	class Iterator
	{
	public:
		Iterator( const uint8_t* data, uint32_t stride, ConversionFunction<T> conversion ) :
			m_data( data ),
			m_stride( stride ),
			m_conversion( conversion )
		{
		}

		Iterator& operator++()
		{
			m_data += m_stride;
			return *this;
		}

		bool operator!=( const Iterator& other ) const
		{
			return m_data != other.m_data;
		}

		T operator*() const
		{
			return m_conversion( m_data );
		}

	private:
		const uint8_t* m_data;
		uint32_t m_stride;
		ConversionFunction<T> m_conversion;
	};

	Iterator begin() const
	{
		return Iterator( m_data, m_stride, m_conversion );
	}

	Iterator end() const
	{
		return Iterator( m_data + m_stride * m_count, m_stride, m_conversion );
	};

	T operator[]( uint32_t index ) const
	{
		return m_conversion( m_data + m_stride * index );
	}

	uint32_t size() const
	{
		return m_count;
	}

	const granny_data_type_definition& type() const
	{
		return m_type;
	}

	bool exists() const
	{
		return m_data != nullptr;
	}

private:
	const uint8_t* m_data = nullptr;
	uint32_t m_stride = 0;
	uint32_t m_count = 0;
	ConversionFunction<T> m_conversion = {};
	granny_data_type_definition m_type = {};
};

struct GrannyIndices
{
public:
	GrannyIndices( const granny_mesh& mesh ) :
		m_mesh( mesh )
	{
	}
	uint32_t operator[]( uint32_t index ) const
	{
		if( m_mesh.PrimaryTopology->Indices16 )
		{
			return m_mesh.PrimaryTopology->Indices16[index];
		}
		return m_mesh.PrimaryTopology->Indices[index];
	}
	uint32_t size() const
	{
		if( m_mesh.PrimaryTopology->Indices16 )
		{
			return m_mesh.PrimaryTopology->Index16Count;
		}
		return m_mesh.PrimaryTopology->IndexCount;
	}

private:
	const granny_mesh& m_mesh;
};

CcpMath::AxisAlignedBox CalculateBounds( const granny_mesh& mesh, uint32_t triFirst, uint32_t triCount )
{
	CcpMath::AxisAlignedBox bounds;
	auto positions = GrannyDataStream<Vector3>( *mesh.PrimaryVertexData, GrannyVertexPositionName );
	auto indices = GrannyIndices( mesh );
	for( uint32_t i = 0; i < triCount * 3; ++i )
	{
		auto v = positions[indices[triFirst * 3 + i]];
		bounds.Include( v );
	}
	return bounds;
}

CcpMath::AxisAlignedBox CalculateBounds( const granny_mesh& mesh )
{
	CcpMath::AxisAlignedBox bounds;
	for( auto v : GrannyDataStream<Vector3>( *mesh.PrimaryVertexData, GrannyVertexPositionName ) )
	{
		bounds.Include( v );
	}
	return bounds;
}

float GetMeshDiameter( const granny_mesh& mesh )
{
	auto aabb = CalculateBounds( mesh );
	if( aabb.IsInitialized() )
	{
		return Length( aabb.m_max - aabb.m_min );
	}
	return 0;
}


std::vector<float> ExtractUvDensities( const granny_mesh& mesh, float discardArea = 0.03f )
{
	auto bytesPerVertex = GrannyGetTotalObjectSize( mesh.PrimaryVertexData->VertexType );

	auto positions = GrannyDataStream<Vector3>( *mesh.PrimaryVertexData, GrannyVertexPositionName );
	GrannyDataStream<Vector4> texCoords[8] = {
		GrannyDataStream<Vector4>( *mesh.PrimaryVertexData, GrannyVertexTextureCoordinatesName ),
		GrannyDataStream<Vector4>( *mesh.PrimaryVertexData, GrannyVertexTextureCoordinatesName "1" ),
		GrannyDataStream<Vector4>( *mesh.PrimaryVertexData, GrannyVertexTextureCoordinatesName "2" ),
		GrannyDataStream<Vector4>( *mesh.PrimaryVertexData, GrannyVertexTextureCoordinatesName "3" ),
		GrannyDataStream<Vector4>( *mesh.PrimaryVertexData, GrannyVertexTextureCoordinatesName "4" ),
		GrannyDataStream<Vector4>( *mesh.PrimaryVertexData, GrannyVertexTextureCoordinatesName "5" ),
		GrannyDataStream<Vector4>( *mesh.PrimaryVertexData, GrannyVertexTextureCoordinatesName "6" ),
		GrannyDataStream<Vector4>( *mesh.PrimaryVertexData, GrannyVertexTextureCoordinatesName "7" )
	};
	if( !texCoords[0].size() )
	{
		texCoords[0] = GrannyDataStream<Vector4>( *mesh.PrimaryVertexData, GrannyVertexTextureCoordinatesName "0" );
	}

	auto diameter = GetMeshDiameter( mesh );

	// First we calculate UV densities for each polygon (8 UV sets max) and store it along with the polygon area.
	// We use the area later to filter out some of the polygons.

	std::vector<std::pair<float, float>> densities[8];
	double totalArea = 0;

	auto indices = GrannyIndices( mesh );
	for( uint32_t idx = 0; idx < indices.size(); idx += 3 )
	{
		Vector3 verts[3];
		Vector4 uvs[3][8] = {};
		for( granny_int32 i = 0; i < 3; ++i )
		{
			granny_int32 vtx = indices[idx + i];
			verts[i] = positions[vtx];
			for( granny_int32 j = 0; j < 8; ++j )
			{
				if( texCoords[j].exists() )
				{
					uvs[i][j] = texCoords[j][vtx];
				}
			}
		}

		float density[8] = {};
		float edges[3];
		bool valid = true;
		for( granny_int32 i = 0; i < 3; ++i )
		{
			auto dx = Length( verts[i] - verts[( i + 1 ) % 3] );
			if( dx == 0 )
			{
				valid = false;
				break;
			}
			edges[i] = dx;
			for( granny_int32 j = 0; j < 8; ++j )
			{
				if( !texCoords[j].exists() )
				{
					continue;
				}
				auto dy = 0.f;
				for( granny_int32 k = 0; k < texCoords[j].type().ArrayWidth; ++k )
				{
					dy += ( uvs[i][j][k] - uvs[( i + 1 ) % 3][j][k] ) * ( uvs[i][j][k] - uvs[( i + 1 ) % 3][j][k] );
				}
				if( dy == 0.f )
				{
					continue;
				}
				dy = sqrt( dy ) * diameter;
				if( i == 0 )
				{
					density[j] = dy / dx;
				}
				else
				{
					density[j] = std::min( density[j], dy / dx );
				}
			}
		}
		if( valid )
		{
			double p = 0.5f * ( edges[0] + edges[1] + edges[2] );
			auto area = sqrt( std::max( p * ( p - edges[0] ) * ( p - edges[1] ) * ( p - edges[2] ), 0. ) );
			totalArea += area;
			for( granny_int32 j = 0; j < 8; ++j )
			{
				if( !texCoords[j].exists() || density[j] == 0 )
				{
					continue;
				}
				densities[j].push_back( { float( area ), density[j] } );
			}
		}
	}

	std::vector<float> uvDensities;

	// Now that we have densities for each polygon, we filter out some of them: removing a specified fraction
	// of total mesh area worth of polygons with the lowest densities. We do this to try to ignore any invalid/hidden/broken polys
	// appear in some models.
	for( granny_int32 j = 0; j < 8; ++j )
	{
		if( !densities[j].empty() )
		{
			std::sort( begin( densities[j] ), end( densities[j] ), []( auto x, auto y ) { return x.second < y.second; } );
			size_t offset = 0;
			float a = 0;
			while( a < totalArea * discardArea )
			{
				a += densities[j][offset].first;
				++offset;
			}
			uvDensities.resize( size_t( j + 1 ) );
			uvDensities[j] = densities[j][offset].second;
		}
	}
	return uvDensities;
}


template <typename T>
struct GrannyArray
{
public:
	GrannyArray( const T* srcArray, granny_int32 count ) :
		m_data( srcArray ),
		m_count( count )
	{
	}

	class Iterator
	{
	public:
		Iterator( const T* data ) :
			m_data( data )
		{
		}

		Iterator& operator++()
		{
			++m_data;
			return *this;
		}

		bool operator!=( const Iterator& other ) const
		{
			return m_data != other.m_data;
		}

		const T& operator*() const
		{
			return *m_data;
		}

	private:
		const T* m_data;
	};

	Iterator begin() const
	{
		return Iterator( m_data );
	}
	Iterator end() const
	{
		return Iterator( m_data + m_count );
	}

private:
	const T* m_data;
	granny_int32 m_count;
};



AnimationCurve ConvertCurve( AnimationChannelTargetType target, const granny_curve2& srcCurve, MemoryAllocator& writer )
{
	AnimationCurve dst;

	const granny_real32* id;
	int elm = 0;
	switch( target )
	{
	case AnimationChannelTargetType::BonePosition:
		id = GrannyCurveIdentityPosition;
		elm = 3;
		dst.valueDimension = 3;
		break;
	case AnimationChannelTargetType::BoneRotation:
		id = GrannyCurveIdentityOrientation;
		elm = 4;
		dst.valueDimension = 4;
		break;
	case AnimationChannelTargetType::BoneScale:
		id = GrannyCurveIdentityScaleShear;
		elm = 9;
		dst.valueDimension = 3;
		break;
	default:
		throw std::runtime_error( "Unsupported target type" );
	}

	if( GrannyCurveIsIdentity( &srcCurve ) )
	{
		return dst;
	}

	granny_curve2* curve = GrannyCurveConvertToDaK32fC32f( &srcCurve, NULL );
	granny_curve_data_da_k32f_c32f* curveData = GrannyCurveGetContentsOfDaK32fC32f( curve );
	if( curveData->ControlCount != curveData->KnotCount * elm )
	{
		throw std::runtime_error( "Invalid curve data" );
	}

	auto degree = curveData->CurveDataHeader.Degree;
	dst.interpolation = Interpolation( degree );

	if( target == AnimationChannelTargetType::BoneRotation )
	{
		//GrannyEnsureQuaternionContinuity( curveData->ControlCount / 4, curveData->Controls );
	}
	dst.knotCount = curveData->KnotCount;

    if ( GrannyCurveIsTypeDaK32fC32f( &srcCurve ) )
    {
		dst.knotType = ElementType::Float32;
		dst.valueType = ElementType::Float32;
		Modify( dst.knots, writer ).insert( dst.knots.end(), reinterpret_cast<uint8_t*>( curveData->Knots ), reinterpret_cast<uint8_t*>( curveData->Knots ) + sizeof( float ) * curveData->KnotCount );
		Modify( dst.values, writer ).insert( dst.values.end(), reinterpret_cast<uint8_t*>( curveData->Controls ), reinterpret_cast<uint8_t*>( curveData->Controls ) + sizeof( float ) * curveData->ControlCount );
	}
    else
    {
		dst.knotType = ElementType::Float16;
		dst.valueType = ElementType::Float16;
		std::vector<Float_16> knots16;
		for( int i = 0; i < curveData->KnotCount; ++i )
		{
			knots16.push_back( Float_16( curveData->Knots[i] ) );
		}
		//knots16.insert( end( knots16 ), curveData->Knots, curveData->Knots + curveData->KnotCount );
		Modify( dst.knots, writer ).insert( dst.knots.end(), reinterpret_cast<uint8_t*>( knots16.data() ), reinterpret_cast<uint8_t*>( knots16.data() ) + sizeof( Float_16 ) * curveData->KnotCount );
		std::vector<Float_16> values16;
		for( int i = 0; i < curveData->ControlCount; ++i )
		{
			values16.push_back( Float_16( curveData->Controls[i] ) );
		}
		//values16.insert( end( values16 ), curveData->Controls, curveData->Controls + curveData->ControlCount );
		Modify( dst.values, writer ).insert( dst.values.end(), reinterpret_cast<uint8_t*>( values16.data() ), reinterpret_cast<uint8_t*>( values16.data() ) + sizeof( Float_16 ) * curveData->ControlCount );
	}

	GrannyFreeCurve( curve );

	return dst;
}

std::pair<int32_t, int32_t> GetMeshMaxScreenSize( const granny_mesh& mesh )
{
	MeshBoundsInfo* mbi = NULL;
	// Look for mesh bounds info in the Granny file
	if( mesh.ExtendedData.Object )
	{
		// The mesh has extended data - ask Granny to convert it to our current version of the bounds
		// info data structure.
		mbi = static_cast<MeshBoundsInfo*>( GrannyConvertTree( mesh.ExtendedData.Type, mesh.ExtendedData.Object, MeshBoundsInfoType, nullptr, nullptr ) );
		if( !mbi->typeName || ( strcmp( mbi->typeName, "MeshBoundsInfo" ) != 0 ) )
		{
			// This extended data doesn't match our expectations
			GrannyFreeBuilderResult( (void*)mbi );
			mbi = NULL;
		}
	}
	if( mbi && mbi->maxScreenSize > 0 )
	{
		return { mbi->maxScreenSize, mbi->sourceMeshIndex };
	}
	return { 0, 0 };
}

void GrannyToCgf( const char* gr2Path, const char* cgfPath )
{
	auto grannyFile = GrannyReadEntireFile( gr2Path );
	auto grannyInfo = GrannyGetFileInfo( grannyFile );

	Data outData;

	MemoryAllocator writer;
	BufferAllocator bufferAllocator;

	std::map<int32_t, int32_t> meshIndices;

	for( auto& srcMesh : GrannyArray( grannyInfo->Meshes, grannyInfo->MeshCount ) )
	{
		if( GetMeshMaxScreenSize( *srcMesh ).first > 0 )
		{
			continue;
		}

		meshIndices[int32_t( &srcMesh - grannyInfo->Meshes )] = int32_t( outData.meshes.size() );

		Mesh& outMesh = Modify( outData.meshes, writer ).emplace_back();
		outMesh.name = writer.AllocateString( srcMesh->Name ? srcMesh->Name : "" );

		{
			uint32_t offset = 0;
			for( auto vt = srcMesh->PrimaryVertexData->VertexType; vt->Type != GrannyEndMember; ++vt )
			{
				Modify( outMesh.decl, writer ).emplace_back( Convert( *vt, offset ) );
			}
		}
		auto stride = GrannyGetTotalObjectSize( srcMesh->PrimaryVertexData->VertexType );
		outMesh.topology = MeshTopology::TriangleList;
		outMesh.vb = bufferAllocator.AddBuffer( srcMesh->PrimaryVertexData->Vertices, srcMesh->PrimaryVertexData->VertexCount * stride, stride );
		if( srcMesh->PrimaryTopology )
		{
			if( srcMesh->PrimaryTopology->Indices16 )
			{
				outMesh.ib = bufferAllocator.AddBuffer( srcMesh->PrimaryTopology->Indices16, srcMesh->PrimaryTopology->Index16Count * 2, 2 );
			}
			else if( srcMesh->PrimaryTopology->Indices )
			{
				outMesh.ib = bufferAllocator.AddBuffer( srcMesh->PrimaryTopology->Indices, srcMesh->PrimaryTopology->IndexCount * 4, 4 );
			}
			outMesh.topology = MeshTopology::TriangleList;

			for( auto& group : GrannyArray( srcMesh->PrimaryTopology->Groups, srcMesh->PrimaryTopology->GroupCount ) )
			{
				auto& area = Modify( outMesh.areas, writer ).emplace_back();
				area.firstElement = group.TriFirst;
				area.elementCount = group.TriCount;
				auto indx = &group - srcMesh->PrimaryTopology->Groups;
				if( indx < srcMesh->MaterialBindingCount )
				{
					auto& binding = srcMesh->MaterialBindings[indx];
					area.name = writer.AllocateString( binding.Material ? binding.Material->Name : "" );
				}
				area.bounds = CalculateBounds( *srcMesh, group.TriFirst, group.TriCount );
			}
			outMesh.bounds = CalculateBounds( *srcMesh );
		}
		else
		{
			outMesh.topology = MeshTopology::PointList;
		}

		for( auto& srcBinding : GrannyArray( srcMesh->BoneBindings, srcMesh->BoneBindingCount ) )
		{
			auto& dstBinding = Modify( outMesh.boneBindings, writer ).emplace_back();
			dstBinding.name = writer.AllocateString( srcBinding.BoneName );
			dstBinding.bounds.m_min.x = srcBinding.OBBMin[0];
			dstBinding.bounds.m_min.y = srcBinding.OBBMin[1];
			dstBinding.bounds.m_min.z = srcBinding.OBBMin[2];
			dstBinding.bounds.m_max.x = srcBinding.OBBMax[0];
			dstBinding.bounds.m_max.y = srcBinding.OBBMax[1];
			dstBinding.bounds.m_max.z = srcBinding.OBBMax[2];
		}

		Modify( outMesh.uvDensities, writer ) = ExtractUvDensities( *srcMesh );

		for( auto& srcMorph : GrannyArray( srcMesh->MorphTargets, srcMesh->MorphTargetCount ) )
		{
			auto& dstMorph = Modify( outMesh.morphTargets, writer ).emplace_back();
			dstMorph.name = writer.AllocateString( srcMorph.ScalarName );
			{
				uint32_t offset = 0;
				for( auto vt = srcMorph.VertexData->VertexType; vt->Type != GrannyEndMember; ++vt )
				{
					Modify( dstMorph.decl, writer ).emplace_back( Convert( *vt, offset ) );
				}
			}
			auto morphStride = GrannyGetTotalObjectSize( srcMesh->PrimaryVertexData->VertexType );
			dstMorph.vb = bufferAllocator.AddBuffer( srcMorph.VertexData->Vertices, srcMorph.VertexData->VertexCount * morphStride, morphStride );
			if( !srcMorph.DataIsDeltas )
			{
				throw std::runtime_error( "Only delta morphs are supported" );
			}
		}

		outMesh.skeleton = 0xff;
		for( int j = 0; j < grannyInfo->ModelCount; ++j )
		{
			for( auto& binding : GrannyArray( grannyInfo->Models[j]->MeshBindings, grannyInfo->Models[j]->MeshBindingCount ) )
			{
				if( binding.Mesh == srcMesh )
				{
					outMesh.skeleton = j;
					break;
				}
			}
		}
	}

	for( auto& srcMesh : GrannyArray( grannyInfo->Meshes, grannyInfo->MeshCount ) )
	{
		auto lodInfo = GetMeshMaxScreenSize( *srcMesh );
		if( lodInfo.first <= 0 )
		{
			continue;
		}
		MeshLod& lod = Modify( outData.meshes[meshIndices[lodInfo.second]].lods, writer ).emplace_back();
		lod.threshold = uint32_t( lodInfo.first );

		auto stride = GrannyGetTotalObjectSize( srcMesh->PrimaryVertexData->VertexType );
		lod.vb = bufferAllocator.AddBuffer( srcMesh->PrimaryVertexData->Vertices, srcMesh->PrimaryVertexData->VertexCount * stride, stride );
		if( srcMesh->PrimaryTopology )
		{
			if( srcMesh->PrimaryTopology->Indices16 )
			{
				lod.ib = bufferAllocator.AddBuffer( srcMesh->PrimaryTopology->Indices16, srcMesh->PrimaryTopology->Index16Count * 2, 2 );
			}
			else if( srcMesh->PrimaryTopology->Indices )
			{
				lod.ib = bufferAllocator.AddBuffer( srcMesh->PrimaryTopology->Indices, srcMesh->PrimaryTopology->IndexCount * 4, 4 );
			}

			for( auto& group : GrannyArray( srcMesh->PrimaryTopology->Groups, srcMesh->PrimaryTopology->GroupCount ) )
			{
				auto& area = Modify( lod.areas, writer ).emplace_back();
				area.firstElement = group.TriFirst;
				area.elementCount = group.TriCount;
			}
		}
	}

	for( auto& srcSkel : GrannyArray( grannyInfo->Skeletons, grannyInfo->SkeletonCount ) )
	{
		auto& outSkel = Modify( outData.skeletons, writer ).emplace_back();
		outSkel.name = writer.AllocateString( srcSkel->Name ? srcSkel->Name : "" );

		for( auto& srcBone : GrannyArray( srcSkel->Bones, srcSkel->BoneCount ) )
		{
			Modify( outSkel.bones, writer ).emplace_back( writer.AllocateString( srcBone.Name ) );
			Modify( outSkel.parents, writer ).emplace_back( uint32_t( srcBone.ParentIndex ) );
			Modify( outSkel.restTransforms, writer ).emplace_back( cmf::Transform{ { srcBone.LocalTransform.Position[0], srcBone.LocalTransform.Position[1], srcBone.LocalTransform.Position[2] }, { srcBone.LocalTransform.Orientation[0], srcBone.LocalTransform.Orientation[1], srcBone.LocalTransform.Orientation[2], srcBone.LocalTransform.Orientation[3] }, { srcBone.LocalTransform.ScaleShear[0][0], srcBone.LocalTransform.ScaleShear[1][1], srcBone.LocalTransform.ScaleShear[2][2] } } );
			Modify( outSkel.invBindTransforms, writer ).emplace_back( *reinterpret_cast<const Matrix*>( srcBone.InverseWorld4x4 ) );
		}
	}

	for( auto& srcAnim : GrannyArray( grannyInfo->Animations, grannyInfo->AnimationCount ) )
	{
		auto& outAnim = Modify( outData.animations, writer ).emplace_back();
		outAnim.name = writer.AllocateString( srcAnim->Name ? srcAnim->Name : "" );
		outAnim.duration = srcAnim->Duration;
		for( auto& srcTrackGroup : GrannyArray( srcAnim->TrackGroups, srcAnim->TrackGroupCount ) )
		{
			for( auto& transformTrack : GrannyArray( srcTrackGroup->TransformTracks, srcTrackGroup->TransformTrackCount ) )
			{
				auto AddCurve = [&]( AnimationChannelTargetType target, const granny_curve2& curve ) {
					auto crv = ConvertCurve( target, curve, writer );
					uint32_t index = 0;
                    for ( const auto& c : outData.curves )
                    {
						if( c.knotType == crv.knotType && c.valueType == crv.valueType && c.valueDimension == crv.valueDimension && c.interpolation == crv.interpolation && c.knotCount == crv.knotCount && memcmp( c.knots.data(), crv.knots.data(), c.knots.size() ) == 0 && memcmp( c.values.data(), crv.values.data(), c.values.size() ) == 0 )
                        {
							return index;
                        }
						++index;
                    }
					Modify( outData.curves, writer ).emplace_back( crv );
					return index;
				};

				Modify( outAnim.channels, writer ).emplace_back( writer.AllocateString( transformTrack.Name ), AnimationChannelTargetType::BonePosition, AddCurve( AnimationChannelTargetType::BonePosition, transformTrack.PositionCurve ) );
				Modify( outAnim.channels, writer ).emplace_back( writer.AllocateString( transformTrack.Name ), AnimationChannelTargetType::BoneRotation, AddCurve( AnimationChannelTargetType::BoneRotation, transformTrack.OrientationCurve ) );
				Modify( outAnim.channels, writer ).emplace_back( writer.AllocateString( transformTrack.Name ), AnimationChannelTargetType::BoneScale, AddCurve( AnimationChannelTargetType::BoneScale, transformTrack.ScaleShearCurve ) );
			}
		}
	}

	auto fileContents = BuildFile( outData, bufferAllocator );

	FILE* f;
	fopen_s( &f, cgfPath, "wb" );
	fwrite( fileContents.data(), fileContents.size(), 1, f );
	fclose( f );
}

void CgfToGranny( const char* cgfPath, const char* gr2Path )
{
	FILE* f;
	fopen_s( &f, cgfPath, "rb" );
	fseek( f, 0, SEEK_END );
	auto size = ftell( f );
	fseek( f, 0, SEEK_SET );
	std::vector<uint8_t> fileContents( size );
	fread( fileContents.data(), size, 1, f );
	fclose( f );

	auto memArena = GrannyNewMemoryArena();

	auto PushString = [memArena]( const String& str ) {
		if( str.empty() )
		{
			return "";
		}
		auto dest = static_cast<char*>( GrannyMemoryArenaPush( memArena, str.size() + 1 ) );
		memcpy( dest, str.data(), str.size() );
		dest[str.size()] = 0;
		return const_cast<const char*>( dest );
	};

	auto PushStdString = [memArena]( const std::string& str ) {
		if( str.empty() )
		{
			return "";
		}
		auto dest = static_cast<char*>( GrannyMemoryArenaPush( memArena, str.size() + 1 ) );
		memcpy( dest, str.data(), str.size() );
		dest[str.size()] = 0;
		return const_cast<const char*>( dest );
	};


	auto data = reinterpret_cast<Data*>( fileContents.data() );
	OffsetsToPointers( *data, data );

	auto grannyInfo = static_cast<granny_file_info*>( GrannyMemoryArenaPush( memArena, sizeof( granny_file_info ) ) );
	*grannyInfo = {};
	grannyInfo->MeshCount = std::accumulate( data->meshes.begin(), data->meshes.end(), 0u, []( uint32_t count, auto& mesh ) { return count + 1 + uint32_t( mesh.lods.size() ); } );
	grannyInfo->Meshes = static_cast<granny_mesh**>( GrannyMemoryArenaPush( memArena, grannyInfo->MeshCount * sizeof( granny_mesh* ) ) );

	uint32_t grannyMeshIndex = 0;

	std::map<int32_t, std::vector<int32_t>> skeletonToMeshMap;

	for( auto& mesh : data->meshes )
	{
		auto grannyMesh = static_cast<granny_mesh*>( GrannyMemoryArenaPush( memArena, sizeof( granny_mesh ) ) );
		*grannyMesh = {};
		grannyMesh->Name = PushString( mesh.name );
		grannyMesh->PrimaryVertexData = static_cast<granny_vertex_data*>( GrannyMemoryArenaPush( memArena, sizeof( granny_vertex_data ) ) );
		*grannyMesh->PrimaryVertexData = {};
		grannyMesh->PrimaryVertexData->VertexCount = mesh.vb.size / mesh.vb.stride;
		grannyMesh->PrimaryVertexData->Vertices = static_cast<uint8_t*>( GrannyMemoryArenaPush( memArena, mesh.vb.size ) );
		memcpy( grannyMesh->PrimaryVertexData->Vertices, fileContents.data() + data->header.bufferOffset + mesh.vb.offset, mesh.vb.size );

		grannyMesh->PrimaryVertexData->VertexType = static_cast<granny_data_type_definition*>( GrannyMemoryArenaPush( memArena, ( mesh.decl.size() + 1 ) * sizeof( granny_data_type_definition ) ) );
		for( size_t i = 0; i < mesh.decl.size(); ++i )
		{
			grannyMesh->PrimaryVertexData->VertexType[i] = Convert( mesh.decl[i], memArena );
		}
		grannyMesh->PrimaryVertexData->VertexType[mesh.decl.size()] = { GrannyEndMember };

		grannyMesh->PrimaryTopology = static_cast<granny_tri_topology*>( GrannyMemoryArenaPush( memArena, sizeof( granny_tri_topology ) ) );
		*grannyMesh->PrimaryTopology = {};
		if( mesh.ib.stride == 2 )
		{
			grannyMesh->PrimaryTopology->Index16Count = mesh.ib.size / 2;
			grannyMesh->PrimaryTopology->Indices16 = static_cast<granny_uint16*>( GrannyMemoryArenaPush( memArena, mesh.ib.size ) );
			memcpy( grannyMesh->PrimaryTopology->Indices16, fileContents.data() + data->header.bufferOffset + mesh.ib.offset, mesh.ib.size );
		}
		else
		{
			grannyMesh->PrimaryTopology->IndexCount = mesh.ib.size / 4;
			grannyMesh->PrimaryTopology->Indices = static_cast<granny_int32*>( GrannyMemoryArenaPush( memArena, mesh.ib.size ) );
			memcpy( grannyMesh->PrimaryTopology->Indices, fileContents.data() + data->header.bufferOffset + mesh.ib.offset, mesh.ib.size );
		}
		grannyMesh->PrimaryTopology->GroupCount = granny_int32( mesh.areas.size() );
		grannyMesh->PrimaryTopology->Groups = static_cast<granny_tri_material_group*>( GrannyMemoryArenaPush( memArena, mesh.areas.size() * sizeof( granny_tri_material_group ) ) );
		for( size_t i = 0; i < mesh.areas.size(); ++i )
		{
			grannyMesh->PrimaryTopology->Groups[i] = {};
			grannyMesh->PrimaryTopology->Groups[i].TriFirst = mesh.areas[i].firstElement;
			grannyMesh->PrimaryTopology->Groups[i].TriCount = mesh.areas[i].elementCount;
		}
		grannyMesh->BoneBindingCount = granny_int32( mesh.boneBindings.size() );
		grannyMesh->BoneBindings = static_cast<granny_bone_binding*>( GrannyMemoryArenaPush( memArena, mesh.boneBindings.size() * sizeof( granny_bone_binding ) ) );
		for( size_t i = 0; i < mesh.boneBindings.size(); ++i )
		{
			grannyMesh->BoneBindings[i] = {};
			grannyMesh->BoneBindings[i].BoneName = PushString( mesh.boneBindings[i].name );
			grannyMesh->BoneBindings[i].OBBMin[0] = mesh.boneBindings[i].bounds.m_min.x;
			grannyMesh->BoneBindings[i].OBBMin[1] = mesh.boneBindings[i].bounds.m_min.y;
			grannyMesh->BoneBindings[i].OBBMin[2] = mesh.boneBindings[i].bounds.m_min.z;
			grannyMesh->BoneBindings[i].OBBMax[0] = mesh.boneBindings[i].bounds.m_max.x;
			grannyMesh->BoneBindings[i].OBBMax[1] = mesh.boneBindings[i].bounds.m_max.y;
			grannyMesh->BoneBindings[i].OBBMax[2] = mesh.boneBindings[i].bounds.m_max.z;
		}
		grannyMesh->MorphTargetCount = granny_int32( mesh.morphTargets.size() );
		grannyMesh->MorphTargets = static_cast<granny_morph_target*>( GrannyMemoryArenaPush( memArena, mesh.morphTargets.size() * sizeof( granny_morph_target ) ) );
		for( size_t i = 0; i < mesh.morphTargets.size(); ++i )
		{
			grannyMesh->MorphTargets[i] = {};
			grannyMesh->MorphTargets[i].ScalarName = PushString( mesh.morphTargets[i].name );
			grannyMesh->MorphTargets[i].VertexData = static_cast<granny_vertex_data*>( GrannyMemoryArenaPush( memArena, sizeof( granny_vertex_data ) ) );
			*grannyMesh->MorphTargets[i].VertexData = {};
			grannyMesh->MorphTargets[i].VertexData->VertexCount = mesh.morphTargets[i].vb.size / mesh.morphTargets[i].vb.stride;
			grannyMesh->MorphTargets[i].VertexData->Vertices = static_cast<uint8_t*>( GrannyMemoryArenaPush( memArena, mesh.morphTargets[i].vb.size ) );
			memcpy( grannyMesh->MorphTargets[i].VertexData->Vertices, fileContents.data() + data->header.bufferOffset + mesh.morphTargets[i].vb.offset, mesh.morphTargets[i].vb.size );

			grannyMesh->MorphTargets[i].VertexData->VertexType = static_cast<granny_data_type_definition*>( GrannyMemoryArenaPush( memArena, ( mesh.morphTargets[i].decl.size() + 1 ) * sizeof( granny_data_type_definition ) ) );
			for( size_t j = 0; j < mesh.morphTargets[i].decl.size(); ++j )
			{
				grannyMesh->MorphTargets[i].VertexData->VertexType[j] = Convert( mesh.morphTargets[i].decl[j], memArena );
			}
			grannyMesh->MorphTargets[i].VertexData->VertexType[mesh.morphTargets[i].decl.size()] = { GrannyEndMember };
			grannyMesh->MorphTargets[i].DataIsDeltas = 1;
		}

		MeshBoundsInfo* mbi = static_cast<MeshBoundsInfo*>( GrannyMemoryArenaPush( memArena, sizeof( MeshBoundsInfo ) ) );
		mbi->typeName = "MeshBoundsInfo";
		memcpy( &mbi->bounds, &mesh.bounds, sizeof( mesh.bounds ) );
		mbi->areaCount = granny_int32( mesh.areas.size() );
		mbi->areaInfos = static_cast<AreaBoundsInfo*>( GrannyMemoryArenaPush( memArena, mesh.areas.size() * sizeof( AreaBoundsInfo ) ) );
		for( size_t i = 0; i < mesh.areas.size(); ++i )
		{
			memcpy( &mbi->areaInfos[i].bounds, &mesh.areas[i].bounds, sizeof( mesh.areas[i].bounds ) );

			std::unordered_set<int> vertexIndicesSeen;
			for( uint32_t j = mesh.areas[i].firstElement; j < mesh.areas[i].firstElement + mesh.areas[i].elementCount; ++j )
			{
				for( int k = 0; k < 3; ++k )
				{
					vertexIndicesSeen.insert( mesh.ib.stride == 2 ? grannyMesh->PrimaryTopology->Indices16[j * 3 + k] : grannyMesh->PrimaryTopology->Indices[j * 3 + k] );
				}
			}
			mbi->areaInfos[i].vertexCount = granny_int32( vertexIndicesSeen.size() );
		}
		mbi->sourceMeshIndex = grannyMeshIndex;
		mbi->maxScreenSize = 0;
		mbi->uvDensityCount = granny_int32( mesh.uvDensities.size() );
		mbi->uvDensities = static_cast<float*>( GrannyMemoryArenaPush( memArena, mesh.uvDensities.size() * sizeof( float ) ) );
		for( size_t i = 0; i < mesh.uvDensities.size(); ++i )
		{
			mbi->uvDensities[i] = mesh.uvDensities[i];
		}
		grannyMesh->ExtendedData.Object = mbi;
		grannyMesh->ExtendedData.Type = MeshBoundsInfoType;

		auto lod0 = grannyMeshIndex;
		auto lod0Mesh = grannyMesh;
		skeletonToMeshMap[mesh.skeleton].push_back( grannyMeshIndex );

		grannyInfo->Meshes[grannyMeshIndex++] = grannyMesh;


		for( auto& lod : mesh.lods )
		{
			auto lodMesh = static_cast<granny_mesh*>( GrannyMemoryArenaPush( memArena, sizeof( granny_mesh ) ) );
			*lodMesh = {};
			auto name = std::string( lod0Mesh->Name ) + " LOD " + std::to_string( lod.threshold );

			lodMesh->Name = static_cast<char*>( GrannyMemoryArenaPush( memArena, name.size() + 1 ) );
			memcpy( const_cast<char*>( lodMesh->Name ), name.c_str(), name.size() + 1 );
			lodMesh->PrimaryVertexData = static_cast<granny_vertex_data*>( GrannyMemoryArenaPush( memArena, sizeof( granny_vertex_data ) ) );
			*lodMesh->PrimaryVertexData = {};
			lodMesh->PrimaryVertexData->VertexCount = lod.vb.size / lod.vb.stride;
			lodMesh->PrimaryVertexData->Vertices = static_cast<uint8_t*>( GrannyMemoryArenaPush( memArena, lod.vb.size ) );
			memcpy( lodMesh->PrimaryVertexData->Vertices, fileContents.data() + data->header.bufferOffset + lod.vb.offset, lod.vb.size );
			lodMesh->PrimaryVertexData->VertexType = lod0Mesh->PrimaryVertexData->VertexType;

			lodMesh->PrimaryTopology = static_cast<granny_tri_topology*>( GrannyMemoryArenaPush( memArena, sizeof( granny_tri_topology ) ) );
			*lodMesh->PrimaryTopology = {};
			if( lod.ib.stride == 2 )
			{
				lodMesh->PrimaryTopology->Index16Count = lod.ib.size / 2;
				lodMesh->PrimaryTopology->Indices16 = static_cast<granny_uint16*>( GrannyMemoryArenaPush( memArena, lod.ib.size ) );
				memcpy( lodMesh->PrimaryTopology->Indices16, fileContents.data() + data->header.bufferOffset + lod.ib.offset, lod.ib.size );
			}
			else
			{
				lodMesh->PrimaryTopology->IndexCount = lod.ib.size / 4;
				lodMesh->PrimaryTopology->Indices = static_cast<granny_int32*>( GrannyMemoryArenaPush( memArena, lod.ib.size ) );
				memcpy( lodMesh->PrimaryTopology->Indices, fileContents.data() + data->header.bufferOffset + lod.ib.offset, lod.ib.size );
			}
			lodMesh->PrimaryTopology->GroupCount = granny_int32( lod.areas.size() );
			lodMesh->PrimaryTopology->Groups = static_cast<granny_tri_material_group*>( GrannyMemoryArenaPush( memArena, lod.areas.size() * sizeof( granny_tri_material_group ) ) );
			for( size_t i = 0; i < lod.areas.size(); ++i )
			{
				lodMesh->PrimaryTopology->Groups[i] = {};
				lodMesh->PrimaryTopology->Groups[i].TriFirst = lod.areas[i].firstElement;
				lodMesh->PrimaryTopology->Groups[i].TriCount = lod.areas[i].elementCount;
			}
			lodMesh->BoneBindingCount = granny_int32( mesh.boneBindings.size() );
			lodMesh->BoneBindings = lod0Mesh->BoneBindings;
			lodMesh->MorphTargetCount = granny_int32( lod.morphTargets.size() );
			lodMesh->MorphTargets = static_cast<granny_morph_target*>( GrannyMemoryArenaPush( memArena, lod.morphTargets.size() * sizeof( granny_morph_target ) ) );
			for( size_t i = 0; i < lod.morphTargets.size(); ++i )
			{
				lodMesh->MorphTargets[i] = {};
				lodMesh->MorphTargets[i].ScalarName = PushString( mesh.morphTargets[i].name );
				lodMesh->MorphTargets[i].VertexData = static_cast<granny_vertex_data*>( GrannyMemoryArenaPush( memArena, sizeof( granny_vertex_data ) ) );
				*lodMesh->MorphTargets[i].VertexData = {};
				lodMesh->MorphTargets[i].VertexData->VertexCount = lod.morphTargets[i].vb.size / lod.morphTargets[i].vb.stride;
				lodMesh->MorphTargets[i].VertexData->Vertices = static_cast<uint8_t*>( GrannyMemoryArenaPush( memArena, lod.morphTargets[i].vb.size ) );
				memcpy( lodMesh->MorphTargets[i].VertexData->Vertices, fileContents.data() + data->header.bufferOffset + lod.morphTargets[i].vb.offset, lod.morphTargets[i].vb.size );
				lodMesh->MorphTargets[i].VertexData->VertexType = lod0Mesh->MorphTargets[i].VertexData->VertexType;
				lodMesh->MorphTargets[i].DataIsDeltas = 1;
			}

			MeshBoundsInfo* mbi = static_cast<MeshBoundsInfo*>( GrannyMemoryArenaPush( memArena, sizeof( MeshBoundsInfo ) ) );
			mbi->typeName = "MeshBoundsInfo";
			memcpy( &mbi->bounds, &mesh.bounds, sizeof( mesh.bounds ) );
			mbi->areaCount = granny_int32( mesh.areas.size() );
			mbi->areaInfos = static_cast<AreaBoundsInfo*>( GrannyMemoryArenaPush( memArena, mesh.areas.size() * sizeof( AreaBoundsInfo ) ) );
			for( size_t i = 0; i < lod.areas.size(); ++i )
			{
				memcpy( &mbi->areaInfos[i].bounds, &mesh.areas[i].bounds, sizeof( mesh.areas[i].bounds ) );
				std::unordered_set<int> vertexIndicesSeen;
				for( uint32_t j = lod.areas[i].firstElement; j < lod.areas[i].firstElement + lod.areas[i].elementCount; ++j )
				{
					for( int k = 0; k < 3; ++k )
					{
						vertexIndicesSeen.insert( lod.ib.stride == 2 ? lodMesh->PrimaryTopology->Indices16[j * 3 + k] : lodMesh->PrimaryTopology->Indices[j * 3 + k] );
					}
				}
				mbi->areaInfos[i].vertexCount = granny_int32( vertexIndicesSeen.size() );
			}
			mbi->sourceMeshIndex = lod0;
			mbi->maxScreenSize = granny_int32( lod.threshold );
			mbi->uvDensityCount = granny_int32( mesh.uvDensities.size() );
			mbi->uvDensities = static_cast<float*>( GrannyMemoryArenaPush( memArena, mesh.uvDensities.size() * sizeof( float ) ) );
			for( size_t i = 0; i < mesh.uvDensities.size(); ++i )
			{
				mbi->uvDensities[i] = mesh.uvDensities[i];
			}
			lodMesh->ExtendedData.Object = mbi;
			lodMesh->ExtendedData.Type = MeshBoundsInfoType;

			skeletonToMeshMap[mesh.skeleton].push_back( grannyMeshIndex );

			grannyInfo->Meshes[grannyMeshIndex++] = lodMesh;
		}
	}

	grannyInfo->SkeletonCount = granny_int32( data->skeletons.size() );
	grannyInfo->Skeletons = static_cast<granny_skeleton**>( GrannyMemoryArenaPush( memArena, grannyInfo->SkeletonCount * sizeof( granny_skeleton* ) ) );

	grannyInfo->ModelCount = granny_int32( data->skeletons.size() );
	grannyInfo->Models = static_cast<granny_model**>( GrannyMemoryArenaPush( memArena, grannyInfo->ModelCount * sizeof( granny_model* ) ) );

	uint32_t skeletonIndex = 0;
	for( auto& skeleton : data->skeletons )
	{
		auto grannySkeleton = static_cast<granny_skeleton*>( GrannyMemoryArenaPush( memArena, sizeof( granny_skeleton ) ) );
		*grannySkeleton = {};
		grannySkeleton->Name = PushString( skeleton.name );
		grannySkeleton->BoneCount = granny_int32( skeleton.bones.size() );
		grannySkeleton->Bones = static_cast<granny_bone*>( GrannyMemoryArenaPush( memArena, skeleton.bones.size() * sizeof( granny_bone ) ) );
		for( size_t i = 0; i < skeleton.bones.size(); ++i )
		{
			grannySkeleton->Bones[i] = {};
			grannySkeleton->Bones[i].Name = PushString( skeleton.bones[i] );
			grannySkeleton->Bones[i].ParentIndex = skeleton.parents[i];
			GrannyMakeIdentity( &grannySkeleton->Bones[i].LocalTransform );

			float scaleShear[9] = {};
			scaleShear[0] = skeleton.restTransforms[i].scale.x;
			scaleShear[4] = skeleton.restTransforms[i].scale.y;
			scaleShear[8] = skeleton.restTransforms[i].scale.z;
			GrannySetTransformWithIdentityCheck(
				&grannySkeleton->Bones[i].LocalTransform,
				&skeleton.restTransforms[i].position.x,
				&skeleton.restTransforms[i].rotation.x,
				scaleShear );
			memcpy( grannySkeleton->Bones[i].InverseWorld4x4, &skeleton.invBindTransforms[i], sizeof( Matrix ) );
		}
		grannyInfo->Skeletons[skeletonIndex] = grannySkeleton;

		auto grannyModel = static_cast<granny_model*>( GrannyMemoryArenaPush( memArena, sizeof( granny_model ) ) );
		*grannyModel = {};
		grannyModel->Name = grannySkeleton->Name;
		grannyModel->Skeleton = grannySkeleton;
		GrannyMakeIdentity( &grannyModel->InitialPlacement );

		if( auto found = skeletonToMeshMap.find( skeletonIndex ); found != end( skeletonToMeshMap ) )
		{
			grannyModel->MeshBindingCount = granny_int32( found->second.size() );
			grannyModel->MeshBindings = static_cast<granny_model_mesh_binding*>( GrannyMemoryArenaPush( memArena, found->second.size() * sizeof( granny_model_mesh_binding ) ) );
			for( size_t i = 0; i < found->second.size(); ++i )
			{
				grannyModel->MeshBindings[i] = {};
				grannyModel->MeshBindings[i].Mesh = grannyInfo->Meshes[found->second[i]];
			}
		}

		grannyInfo->Models[skeletonIndex] = grannyModel;
		++skeletonIndex;
	}

	grannyInfo->AnimationCount = granny_int32( data->animations.size() );
	grannyInfo->Animations = static_cast<granny_animation**>( GrannyMemoryArenaPush( memArena, grannyInfo->AnimationCount * sizeof( granny_animation* ) ) );
	uint32_t animationIndex = 0;


	for( auto& animation : data->animations )
	{
		struct TransformChannels
		{
			size_t position = 0xffffffff;
			size_t orientation = 0xffffffff;
			size_t scale = 0xffffffff;
		};

		std::map<std::string, TransformChannels> transformChannels;
		for( size_t i = 0; i < animation.channels.size(); ++i )
		{
			auto& channel = transformChannels[std::string( animation.channels[i].target.data(), animation.channels[i].target.size() )];
			switch( animation.channels[i].targetType )
			{
			case AnimationChannelTargetType::BonePosition:
				channel.position = i;
				break;
			case AnimationChannelTargetType::BoneRotation:
				channel.orientation = i;
				break;
			case AnimationChannelTargetType::BoneScale:
				channel.scale = i;
				break;
			}
		}

		auto grannyAnimation = static_cast<granny_animation*>( GrannyMemoryArenaPush( memArena, sizeof( granny_animation ) ) );
		*grannyAnimation = {};
		grannyAnimation->Name = PushString( animation.name );
		grannyAnimation->Duration = animation.duration;
		grannyAnimation->DefaultLoopCount = 1;
		grannyAnimation->Flags = 1;
		grannyAnimation->TrackGroupCount = 1;
		grannyAnimation->TrackGroups = static_cast<granny_track_group**>( GrannyMemoryArenaPush( memArena, grannyAnimation->TrackGroupCount * sizeof( granny_track_group* ) ) );
		auto grannyTrackGroup = static_cast<granny_track_group*>( GrannyMemoryArenaPush( memArena, sizeof( granny_track_group ) ) );
		*grannyTrackGroup = {};
		if( grannyInfo->ModelCount > 0 )
		{
			grannyTrackGroup->Name = grannyInfo->Models[0]->Name;
		}
		GrannyMakeIdentity( &grannyTrackGroup->InitialPlacement );
		grannyTrackGroup->Flags = 6; // GrannyTrackGroupIsSorted;
		grannyTrackGroup->TransformTrackCount = granny_int32( transformChannels.size() );
		grannyTrackGroup->TransformTracks = static_cast<granny_transform_track*>( GrannyMemoryArenaPush( memArena, grannyTrackGroup->TransformTrackCount * sizeof( granny_transform_track ) ) );
		int i = 0;
		for( auto& [target, channels] : transformChannels )
		{
			grannyTrackGroup->TransformTracks[i] = {};
			grannyTrackGroup->TransformTracks[i].Name = PushStdString( target );

			auto BuildCurve = [&]( const AnimationCurve& channel ) {
				auto builder = GrannyBeginCurve( GrannyCurveDataDaK32fC32fType, int( channel.interpolation ), channel.valueDimension, channel.knotCount );
                if ( channel.knotType == ElementType::Float32 )
                {
					GrannyPushCurveKnotArray( builder, reinterpret_cast<const float*>( channel.knots.begin() ) );
				}
                else
                {
					auto knots = static_cast<float*>( GrannyMemoryArenaPush( memArena, channel.knots.size() / sizeof( Float_16 ) * sizeof( float ) ) );
					for( size_t i = 0; i < channel.knots.size() / sizeof( Float_16 ); ++i )
					{
						knots[i] = reinterpret_cast<const Float_16*>( channel.knots.begin() )[i];
					}
					GrannyPushCurveKnotArray( builder, knots );
				}
				if( channel.valueType == ElementType::Float32 )
				{
					GrannyPushCurveControlArray( builder, reinterpret_cast<const float*>( channel.values.begin() ) );
				}
				else
				{
					auto values = static_cast<float*>( GrannyMemoryArenaPush( memArena, channel.values.size() / sizeof( Float_16 ) * sizeof( float ) ) );
					for( size_t i = 0; i < channel.values.size() / sizeof( Float_16 ); ++i )
					{
						values[i] = reinterpret_cast<const Float_16*>( channel.values.begin() )[i];
					}
					GrannyPushCurveControlArray( builder, values );
				}
				return *GrannyEndCurveInPlace( builder, GrannyMemoryArenaPush( memArena, GrannyGetResultingCurveSize( builder ) ) );
			};
			grannyTrackGroup->TransformTracks[i].PositionCurve = BuildCurve( data->curves[animation.channels[channels.position].curveIndex] );
			grannyTrackGroup->TransformTracks[i].OrientationCurve = BuildCurve( data->curves[animation.channels[channels.orientation].curveIndex] );
			grannyTrackGroup->TransformTracks[i].ScaleShearCurve = BuildCurve( data->curves[animation.channels[channels.scale].curveIndex] );
			++i;
		}
		grannyAnimation->TrackGroups[0] = grannyTrackGroup;
		grannyInfo->Animations[animationIndex++] = grannyAnimation;
	}

	granny_file_builder* builder = GrannyBeginFile(
		1,
		GrannyCurrentGRNStandardTag,
		GrannyGRNFileMV_ThisPlatform,
		GrannyGetTemporaryDirectory(),
		"prefix2" );
	granny_file_data_tree_writer* writer = GrannyBeginFileDataTreeWriting( GrannyFileInfoType, grannyInfo, 0, 0 );

	GrannyWriteDataTreeToFileBuilder( writer, builder );
	GrannyEndFileDataTreeWriting( writer );

	GrannyEndFile( builder, gr2Path );
}

int main()
{
	//GrannyToCgf( "d:\\perforce\\eve\\branches\\sandbox\\TE-CARBON1\\eve\\client\\res\\dx9\\model\\ship\\amarr\\battlecruiser\\abc1\\abc1_t1.gr2", "d:\\temp\\abc1_t1.cgf" );
	//CgfToGranny( "d:\\temp\\abc1_t1.cgf", "d:\\temp\\abc1_t1.gr2" );

	GrannyToCgf( "d:\\perforce\\depot\\content\\EVE\\Res\\dx9\\model\\ship\\caldari\\Destroyer\\CDe3\\CDe3_T3.gr2", "d:\\temp\\CDe3_T3.cgf" );
	CgfToGranny( "d:\\temp\\CDe3_T3.cgf", "d:\\temp\\CDe3_T3.gr2" );

	return 0;
}


// Copyright © 2026 CCP ehf.

#include "commands.h"
#include "cmffile.h"
#include "cmf/bufferstreams.h"
#include "cmf/declutils.h"

#include <nlohmann/json.hpp>

// NOLINTBEGIN(readability-identifier-naming, misc-use-anonymous-namespace)
static void to_json( nlohmann::json& j, const Vector3& v )
{
	j = nlohmann::json{ v.x, v.y, v.z };
}

static void to_json( nlohmann::json& j, const Quaternion& v )
{
	j = nlohmann::json{ v.x, v.y, v.z, v.w };
}

static void to_json( nlohmann::json& j, const CcpMath::AxisAlignedBox& aabb )
{
	j = nlohmann::json{
		{ "min", aabb.m_min },
		{ "max", aabb.m_max }
	};
}

static void to_json( nlohmann::json& j, const Matrix& m )
{
	j = nlohmann::json{
		{ m._11, m._12, m._13, m._14 },
		{ m._21, m._22, m._23, m._24 },
		{ m._31, m._32, m._33, m._34 },
		{ m._41, m._42, m._43, m._44 }
	};
}
// NOLINTEND(readability-identifier-naming, misc-use-anonymous-namespace)


namespace
{

struct DumpJsonOptions
{
	std::string path;
	bool header = false;
	bool data = false;
	bool metadata = false;
	bool curves = false;
	bool buffers = false;
	int indent = -1;
};


template <typename T>
void DumpCmfData( nlohmann::json& json, T& data )
{
	if constexpr( std::is_same_v<T, cmf::String> )
	{
		json = cmf::ToStdStringView( data );
	}
	else if constexpr( std::is_base_of_v<cmf::SpanRepr, T> )
	{
		json = nlohmann::json::array();
		for( auto& element : data )
		{
			nlohmann::json elementJson;
			DumpCmfData( elementJson, element );
			json.push_back( std::move( elementJson ) );
		}
	}
	else if constexpr( cmf::IsCmfDataType<T>::value )
	{
		nlohmann::json dataJson;
		cmf::EnumerateMembers( data, [&dataJson]( auto&&, auto& value, const char* name ) {
			DumpCmfData( dataJson[name], value );
		} );
		json = std::move( dataJson );
	}
	else
	{
		to_json( json, data );
	}
}

void DumpCurveKnots( nlohmann::json& json, const cmf::AnimationCurve& curve )
{
	cmf::VertexElement element = {};
	element.type = curve.knotType;
	element.elementCount = 1;
	const auto stride = cmf::GetVertexElementSize( element );
	const cmf::ConstBufferElementStream<float> knotStream{ element, curve.knots.data(), uint32_t( curve.knots.size() / stride ), stride };
	for( auto knot : knotStream )
	{
		json.push_back( knot );
	}
}

void AddValueToJson( nlohmann::json& json, const Vector4& value, uint8_t dimension )
{
	switch( dimension )
	{
	case 1:
		json.push_back( value.x );
		break;
	case 2:
		json.push_back( { value.x, value.y } );
		break;
	case 3:
		json.push_back( { value.x, value.y, value.z } );
		break;
	default:
		json.push_back( value );
		break;
	}
}

void DumpCurveValues( nlohmann::json& json, const cmf::AnimationCurve& curve )
{
	cmf::VertexElement element = {};
	element.type = curve.valueType;
	element.elementCount = curve.valueDimension;
	const auto stride = cmf::GetVertexElementSize( element );
	const cmf::ConstBufferElementStream<Vector4> valueStream{ element, curve.values.data(), uint32_t( curve.values.size() / stride ), stride };
	for( auto value : valueStream )
	{
		AddValueToJson( json, value, curve.valueDimension );
	}
}


void DumpCurveData( nlohmann::json& json, const cmf::Span<cmf::AnimationCurve>& curves )
{
	for( const auto& curve : curves )
	{
		auto& jsonCurve = json.at( std::distance( curves.begin(), &curve ) );
		jsonCurve["knots"] = {};
		jsonCurve["values"] = {};
		DumpCurveKnots( jsonCurve["knots"], curve );
		DumpCurveValues( jsonCurve["values"], curve );
	}
}

void DumpIndexBuffer( nlohmann::json& json, const cmf::BufferView& ib, const cmf::BufferManager& buffers )
{
	auto& data = json["data"];
	for( auto idx : cmf::ConstIndexBufferStream( ib, buffers ) )
	{
		data.push_back( idx );
	}
}

void DumpVertexBuffer( nlohmann::json& json, const cmf::BufferView& vb, const cmf::Span<cmf::VertexElement>& decl, const cmf::BufferManager& buffers )
{
	auto& data = json["data"];
	for( const auto& element : decl )
	{
		uint32_t index = 0;
		for( auto vertex : cmf::ConstBufferElementStream<Vector4>( element, vb, buffers ) )
		{
			AddValueToJson( data[index++], vertex, element.elementCount );
		}
	}
}

void DumpBufferContents( nlohmann::json& json, const cmf::Data& data, const cmf::BufferManager& buffers )
{
	for( const auto& mesh : data.meshes )
	{
		for( const auto& lod : mesh.lods )
		{
			auto& jsonLod = json.at( "meshes" ).at( std::distance( data.meshes.begin(), &mesh ) ).at( "lods" ).at( std::distance( mesh.lods.begin(), &lod ) );
			DumpIndexBuffer( jsonLod.at( "ib" ), lod.ib, buffers );
			DumpVertexBuffer( jsonLod.at( "vb" ), lod.vb, mesh.decl, buffers );
			for( const auto& morphTarget : lod.morphTargets )
			{
				DumpVertexBuffer( jsonLod.at( "morphTargets" ).at( std::distance( lod.morphTargets.begin(), &morphTarget ) ), morphTarget.vb, mesh.morphTargets.decl, buffers );
			}
		}
	}
}

void DumpJson( CLI::App& app, DumpJsonOptions& options )
{
	app.add_flag( "--header", options.header, "Include file header" );
	app.add_flag( "--data", options.data, "Include data section" );
	app.add_flag( "--metadata", options.metadata, "Include metadata section" );
	app.add_flag( "--curves", options.curves, "Convert animation curve data to numbers; otherwise dump as bytes; only applicable if --data is specified" );
	app.add_flag( "--buffers", options.buffers, "Include buffer view contents unpacked from buffers; only applicable if --data is specified" );
	app.add_option( "--indent", options.indent, "Number of spaces to indent for pretty printing; if not specified, output will be minified" );
	app.add_option( "file", options.path, "Path to CMF file" )->required()->check( CLI::ExistingFile );

	app.final_callback( [&options]() {
		CmfFile file( options.path );

		nlohmann::json output = {};
		if( options.header )
		{
			DumpCmfData( output["header"], file.GetHeader() );
		}
		if( options.data )
		{
			DumpCmfData( output["data"], file.GetData() );
			if( options.curves && !file.GetData().animations.empty() )
			{
				for( uint32_t i = 0; i < file.GetData().animations.size(); ++i )
				{
					auto& jsonAnim = output.at( "data" ).at( "animations" ).at( i );
					DumpCurveData( jsonAnim.at( "curves" ), file.GetData().animations[i].curves );
				}
			}
			if( options.buffers )
			{
				DumpBufferContents( output.at( "data" ), file.GetData(), file.GetBufferManager() );
			}
		}
		if( options.metadata )
		{
			DumpCmfData( output["metadata"], file.GetMetadata() );
		}
		printf( "%s\n", output.dump( options.indent ).c_str() ); // NOLINT(cppcoreguidelines-pro-type-vararg)
	} );
}
}

REGISTER_COMMAND( "dumpjson", "Outputs contents of a CMF file as json data", &DumpJson );

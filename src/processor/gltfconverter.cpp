#include "commands.h"
#include "cmffile.h"
#include "cmf/tangents.h"
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tiny_gltf.h>

#include <cfloat>
#include <stdexcept>

namespace
{
struct GLTFOptions
{
	std::string srcPath;
	std::string dstPath;
	bool combinedFile = false;
};

struct CMFUsageAttribute
{
	cmf::Usage usage;
	const char* prefix;
};

// Find elements that do not require usageIndex and only exist in the data stream once
const cmf::VertexElement* FindSingleUsageElement( cmf::Span<cmf::VertexElement> decl, cmf::Usage usage )
{
	for( const auto& element : decl )
	{
		if( element.usage == usage )
			return &element;
	}
	return nullptr;
}

void AlignBuffer( tinygltf::Buffer& buf, size_t alignment )
{
	while( buf.data.size() % alignment != 0 )
		buf.data.push_back( 0 );
}

int GetElementTypeSize( cmf::ElementType element )
{
	switch( element )
	{
	case cmf::ElementType::Float32:
		return sizeof( uint32_t );
	case cmf::ElementType::Float16:
	case cmf::ElementType::UInt16Norm:
	case cmf::ElementType::UInt16:
	case cmf::ElementType::Int16Norm:
	case cmf::ElementType::Int16:
		return sizeof( uint16_t );
	case cmf::ElementType::UInt8Norm:
	case cmf::ElementType::UInt8:
	case cmf::ElementType::Int8Norm:
	case cmf::ElementType::Int8:
		return sizeof( uint8_t );
	}
	return 1;
}

int GetGltfComponentType( cmf::ElementType element )
{
	switch( element )
	{
	case cmf::ElementType::Float32:
		return TINYGLTF_COMPONENT_TYPE_FLOAT;
	case cmf::ElementType::Float16:
	case cmf::ElementType::UInt16Norm:
	case cmf::ElementType::UInt16:
	case cmf::ElementType::Int16Norm:
	case cmf::ElementType::Int16:
		return TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
	case cmf::ElementType::UInt8Norm:
	case cmf::ElementType::UInt8:
	case cmf::ElementType::Int8Norm:
	case cmf::ElementType::Int8:
		return TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
	}
	return 1;
}

std::string GenerateAttributeName( const std::map<std::string, int>& usedAtributeNames, std::string name, int index )
{
	// Continue for a reasonable amount of similarly named attributes.
	// VK and GL guarantee at least 16 elements, so we will provide a worse case senario
	for( int i = index; i < 16; i++ )
	{
		std::string newName = name;
		if( i > 0 )
		{
			newName += "_" + std::to_string( i );
		}
		if( usedAtributeNames.find( newName ) == usedAtributeNames.end() )
		{
			return newName;
		}
	}
	throw std::runtime_error( "Could not find a unique attribute name for '" + name + "' starting at index " + std::to_string( index ) );
}


int AddVertexAttribute( const uint8_t* vbBytes, uint32_t vertexCount, uint32_t stride, const cmf::VertexElement& element, tinygltf::Buffer& gltfBuffer, tinygltf::Model& model, bool normalized = false )
{
	const int componentCount = element.elementCount;
	const size_t componentSize = GetElementTypeSize( element.type );
	const int gltfComponentType = GetGltfComponentType( element.type );

	int gltfType = TINYGLTF_TYPE_SCALAR;
	switch( componentCount )
	{
	case 2:
		gltfType = TINYGLTF_TYPE_VEC2;
		break;
	case 3:
		gltfType = TINYGLTF_TYPE_VEC3;
		break;
	case 4:
		gltfType = TINYGLTF_TYPE_VEC4;
		break;
	default:
		gltfType = TINYGLTF_TYPE_SCALAR;
		break;
	}

	AlignBuffer( gltfBuffer, componentSize );
	const size_t byteOffset = gltfBuffer.data.size();
	const size_t byteLength = static_cast<size_t>( vertexCount ) * componentCount * componentSize;
	gltfBuffer.data.resize( byteOffset + byteLength );

	std::vector<double> minVals( componentCount, DBL_MAX );
	std::vector<double> maxVals( componentCount, -DBL_MAX );

	uint8_t* dst = gltfBuffer.data.data() + byteOffset;

	if( element.type == cmf::ElementType::Float32 )
	{
		float* floatDst = reinterpret_cast<float*>( dst );
		for( uint32_t v = 0; v < vertexCount; ++v )
		{
			const float* src = reinterpret_cast<const float*>( vbBytes + v * stride + element.offset );
			for( int k = 0; k < componentCount; ++k )
			{
				floatDst[v * componentCount + k] = src[k];
				minVals[k] = std::min( minVals[k], static_cast<double>( src[k] ) );
				maxVals[k] = std::max( maxVals[k], static_cast<double>( src[k] ) );
			}
		}
	}
	else if( element.type == cmf::ElementType::Float16 )
	{
		float* floatDst = reinterpret_cast<float*>( dst );
		for( uint32_t v = 0; v < vertexCount; ++v )
		{
			const uint16_t* src = reinterpret_cast<const uint16_t*>( vbBytes + v * stride + element.offset );
			for( int k = 0; k < componentCount; ++k )
			{
				const float val = static_cast<float>( Float_16( src[k] ) );
				floatDst[v * componentCount + k] = val;
				minVals[k] = std::min( minVals[k], static_cast<double>( val ) );
				maxVals[k] = std::max( maxVals[k], static_cast<double>( val ) );
			}
		}
	}
	else
	{
		for( uint32_t v = 0; v < vertexCount; ++v )
		{
			const uint8_t* src = vbBytes + v * stride + element.offset;
			memcpy( dst + v * componentCount * componentSize, src, componentCount * componentSize );
		}
	}

	const int bvIdx = static_cast<int>( model.bufferViews.size() );
	{
		tinygltf::BufferView bv;
		bv.buffer = 0;
		bv.byteOffset = byteOffset;
		bv.byteLength = byteLength;
		bv.target = TINYGLTF_TARGET_ARRAY_BUFFER;
		model.bufferViews.push_back( bv );
	}

	const int accIdx = static_cast<int>( model.accessors.size() );
	{
		tinygltf::Accessor acc;
		acc.bufferView = bvIdx;
		acc.byteOffset = 0;
		acc.componentType = gltfComponentType;
		acc.normalized = normalized;
		acc.type = gltfType;
		acc.count = vertexCount;
		if( vertexCount > 0 && ( element.type == cmf::ElementType::Float32 || element.type == cmf::ElementType::Float16 ) )
		{
			acc.minValues = minVals;
			acc.maxValues = maxVals;
		}
		model.accessors.push_back( acc );
	}
	return accIdx;
}

float GenerateBinormalSign( const Vector3& normal, const Vector3& tangent, const Vector3& bitangent )
{
	const float cx = normal.y * tangent.z - normal.z * tangent.y;
	const float cy = normal.z * tangent.x - normal.x * tangent.z;
	const float cz = normal.x * tangent.y - normal.y * tangent.x;
	return ( cx * bitangent.x + cy * bitangent.y + cz * bitangent.z ) < 0.0f ? -1.0f : 1.0f;
}

int AddTangentAttribute( const uint8_t* vbBytes, uint32_t vertexCount, uint32_t stride, const cmf::VertexElement& tangentElem, cmf::Span<cmf::VertexElement> decl, tinygltf::Buffer& gltfBuffer, tinygltf::Model& model )
{
	const auto* normalElem = FindSingleUsageElement( decl, cmf::Usage::Normal );
	const auto* binormalElem = FindSingleUsageElement( decl, cmf::Usage::Binormal );

	if( !normalElem || normalElem->type != cmf::ElementType::Float32 || normalElem->elementCount < 3 )
	{
		throw std::runtime_error( "Cannot reconstruct TANGENT w: missing or unsupported Normal element" );
	}
	if( !binormalElem || binormalElem->type != cmf::ElementType::Float32 || binormalElem->elementCount < 3 )
	{
		throw std::runtime_error( "Cannot reconstruct TANGENT w: missing or unsupported Binormal element" );
	}

	// Rebuild the tangent buffer storing the binormal in the W component according to the spec under "TANGENT"
	// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#meshes-overview
	std::vector<float> tanBuffer( vertexCount * 4 );
	for( uint32_t v = 0; v < vertexCount; ++v )
	{
		const float* T = reinterpret_cast<const float*>( vbBytes + v * stride + tangentElem.offset );
		const float* N = reinterpret_cast<const float*>( vbBytes + v * stride + normalElem->offset );
		const float* B = reinterpret_cast<const float*>( vbBytes + v * stride + binormalElem->offset );
		const float w = GenerateBinormalSign( Vector3( N[0], N[1], N[2] ), Vector3( T[0], T[1], T[2] ), Vector3( B[0], B[1], B[2] ) );
		tanBuffer[v * 4 + 0] = T[0];
		tanBuffer[v * 4 + 1] = T[1];
		tanBuffer[v * 4 + 2] = T[2];
		tanBuffer[v * 4 + 3] = w;
	}

	cmf::VertexElement tanElem{};
	tanElem.type = cmf::ElementType::Float32;
	tanElem.elementCount = 4;
	tanElem.offset = 0;

	return AddVertexAttribute( reinterpret_cast<const uint8_t*>( tanBuffer.data() ), vertexCount, 4 * sizeof( float ), tanElem, gltfBuffer, model );
}

std::pair<int, int> AddPackedTangentAttribute( const uint8_t* vbBytes, uint32_t vertexCount, uint32_t stride, const cmf::VertexElement& element, tinygltf::Buffer& gltfBuffer, tinygltf::Model& model )
{
	const bool isQuaternion = element.usage == cmf::Usage::PackedTangent;
	const cmf::TangentCompression compression = isQuaternion ? cmf::TangentCompression::PackedTangent : cmf::TangentCompression::PackedTangentLegacy;

	std::vector<float> normalBuffer( vertexCount * 3 );
	std::vector<float> tangentBuffer( vertexCount * 4 );

	for( uint32_t v = 0; v < vertexCount; ++v )
	{
		const uint8_t* src = vbBytes + v * stride + element.offset;

		auto ReadTangentComponent = [&]( const uint8_t* buffer, int k ) -> float {
			switch( element.type )
			{
			// PackedTangent
			case cmf::ElementType::Int16Norm:
				return std::max( reinterpret_cast<const int16_t*>( buffer )[k] / 32767.0f, -1.0f );
			case cmf::ElementType::Int8Norm:
				return std::max( reinterpret_cast<const int8_t*>( buffer )[k] / 127.0f, -1.0f );
			// PackedTangentLegacy
			case cmf::ElementType::UInt16Norm:
				return reinterpret_cast<const uint16_t*>( buffer )[k] / 65535.0f;
			case cmf::ElementType::UInt8Norm:
				return reinterpret_cast<const uint8_t*>( buffer )[k] / 255.0f;
			default:
				return 0.0f;
			}
		};

		Vector4 packed( ReadTangentComponent( src, 0 ), ReadTangentComponent( src, 1 ), ReadTangentComponent( src, 2 ), ReadTangentComponent( src, 3 ) );

		auto [normal, tangent, bitangent] = cmf::UnpackTangents( compression, packed );

		// Convert the CMF normal, tangent and bitangent into gltf compatible formats with packed bitangent
		// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#meshes-overview
		const float w = GenerateBinormalSign( normal, tangent, bitangent );

		normalBuffer[v * 3 + 0] = normal.x;
		normalBuffer[v * 3 + 1] = normal.y;
		normalBuffer[v * 3 + 2] = normal.z;

		tangentBuffer[v * 4 + 0] = tangent.x;
		tangentBuffer[v * 4 + 1] = tangent.y;
		tangentBuffer[v * 4 + 2] = tangent.z;
		tangentBuffer[v * 4 + 3] = w;
	}

	cmf::VertexElement normalElem{};
	normalElem.type = cmf::ElementType::Float32;
	normalElem.elementCount = 3;
	normalElem.offset = 0;

	cmf::VertexElement tanElem{};
	tanElem.type = cmf::ElementType::Float32;
	tanElem.elementCount = 4;
	tanElem.offset = 0;

	const int normalAccIdx = AddVertexAttribute( reinterpret_cast<const uint8_t*>( normalBuffer.data() ), vertexCount, 3 * sizeof( float ), normalElem, gltfBuffer, model );
	const int tangentAccIdx = AddVertexAttribute( reinterpret_cast<const uint8_t*>( tangentBuffer.data() ), vertexCount, 4 * sizeof( float ), tanElem, gltfBuffer, model );

	return { normalAccIdx, tangentAccIdx };
}

void AddMesh( const cmf::v1::Mesh& mesh, cmf::BufferManager& bufferManager, tinygltf::Buffer& gltfBuffer, tinygltf::Model& model, tinygltf::Scene& scene )
{
	if( mesh.lods.empty() )
		return;

	const auto* posElement = FindSingleUsageElement( mesh.decl, cmf::Usage::Position );
	if( !posElement || ( posElement->type != cmf::ElementType::Float32 && posElement->type != cmf::ElementType::Float16 ) || posElement->elementCount != 3 )
	{
		fprintf( stderr, "Mesh '%.*s' has unsupported position format; skipping\n", static_cast<int>( mesh.name.size() ), mesh.name.begin() );
		return;
	}

	const std::string meshName( mesh.name.begin(), mesh.name.end() );
	std::vector<int> lodNodeIndices;

	for( const auto& lod : mesh.lods )
	{
		const uint32_t vertexCount = lod.vb.stride > 0 ? lod.vb.size / lod.vb.stride : 0;
		const auto* vbBytes = static_cast<const uint8_t*>( bufferManager.GetData( lod.vb ) );

		// Vertex attributes
		tinygltf::Primitive prim;
		switch( mesh.topology )
		{
		case cmf::v1::MeshTopology::PointList: {
			prim.mode = TINYGLTF_MODE_POINTS;
			break;
		}
		case cmf::v1::MeshTopology::TriangleList: {
			prim.mode = TINYGLTF_MODE_TRIANGLES;
			break;
		}
		default: {
			fprintf( stderr, "Unsupported MeshTopology\n" );
			return;
		}
		}

		// Write indices into glTF buffer
		if( lod.ib.stride > 0 )
		{
			const uint32_t indexCount = lod.ib.size / lod.ib.stride;

			AlignBuffer( gltfBuffer, lod.ib.stride );
			const size_t indexByteOffset = gltfBuffer.data.size();
			const size_t indexByteLength = static_cast<size_t>( indexCount ) * lod.ib.stride;
			const auto* ibBytes = static_cast<const uint8_t*>( bufferManager.GetData( lod.ib ) );
			gltfBuffer.data.resize( indexByteOffset + indexByteLength );
			memcpy( gltfBuffer.data.data() + indexByteOffset, ibBytes, indexByteLength );

			const int indexBVIdx = static_cast<int>( model.bufferViews.size() );
			{
				tinygltf::BufferView bv;
				bv.buffer = 0;
				bv.byteOffset = indexByteOffset;
				bv.byteLength = indexByteLength;
				bv.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;
				model.bufferViews.push_back( bv );
			}

			const int indexAccIdx = static_cast<int>( model.accessors.size() );
			{
				tinygltf::Accessor acc;
				acc.bufferView = indexBVIdx;
				acc.byteOffset = 0;
				acc.componentType = lod.ib.stride == 2 ? TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT : TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
				acc.type = TINYGLTF_TYPE_SCALAR;
				acc.count = indexCount;
				model.accessors.push_back( acc );
			}

			prim.indices = indexAccIdx;
		}

		// Position the packed tangents before the normal and tangent so that when we come in and add normals and tangents they are
		// offset from the packed data by (packed data count + index), rather than normals and tangents being populated first
		// and changing the pairing of {(N + T),...} in the output data.
		CMFUsageAttribute kMultiAttribs[] = {
			{ cmf::Usage::Position, "POSITION" },
			{ cmf::Usage::PackedTangent, "" },
			{ cmf::Usage::PackedTangentLegacy, "" },
			{ cmf::Usage::Normal, "NORMAL" },
			{ cmf::Usage::Tangent, "TANGENT" },
			{ cmf::Usage::TexCoord, "TEXCOORD" },
			{ cmf::Usage::Color, "COLOR" },
		};
		for( const auto& attrib : kMultiAttribs )
		{
			for( const auto& elem : mesh.decl )
			{
				if( elem.usage == attrib.usage && attrib.usage == cmf::Usage::Tangent )
				{
					std::string tangentName = GenerateAttributeName( prim.attributes, attrib.prefix, elem.usageIndex );
					prim.attributes[tangentName] = AddTangentAttribute( vbBytes, vertexCount, lod.vb.stride, elem, mesh.decl, gltfBuffer, model );
				}
				else if( elem.usage == attrib.usage && ( attrib.usage == cmf::Usage::PackedTangent || attrib.usage == cmf::Usage::PackedTangentLegacy ) )
				{
					auto [normalAccIdx, tangentAccIdx] = AddPackedTangentAttribute( vbBytes, vertexCount, lod.vb.stride, elem, gltfBuffer, model );

					std::string normalName = GenerateAttributeName( prim.attributes, "NORMAL", elem.usageIndex );
					std::string tangentName = GenerateAttributeName( prim.attributes, "TANGENT", elem.usageIndex );
					prim.attributes[normalName] = normalAccIdx;
					prim.attributes[tangentName] = tangentAccIdx;
				}
				else if( elem.usage == attrib.usage && elem.type == cmf::ElementType::Float32 )
				{
					std::string name = GenerateAttributeName( prim.attributes, std::string( attrib.prefix ), elem.usageIndex );

					prim.attributes[name] = AddVertexAttribute( vbBytes, vertexCount, lod.vb.stride, elem, gltfBuffer, model );
				}
			}
		}

		// Integer attributes (JOINTS_n)
		for( const auto& elem : mesh.decl )
		{
			if( elem.usage == cmf::Usage::BoneIndices &&
				( elem.type == cmf::ElementType::UInt8 || elem.type == cmf::ElementType::UInt16 ) )
			{
				const std::string name = GenerateAttributeName( prim.attributes, "JOINTS_", elem.usageIndex );
				prim.attributes[name] = AddVertexAttribute( vbBytes, vertexCount, lod.vb.stride, elem, gltfBuffer, model );
			}
		}

		// Normalized integer attributes (WEIGHTS_n for non-float bone weights)
		for( const auto& elem : mesh.decl )
		{
			if( elem.usage == cmf::Usage::BoneWeights &&
				( elem.type == cmf::ElementType::UInt8 || elem.type == cmf::ElementType::UInt8Norm ||
				  elem.type == cmf::ElementType::UInt16 || elem.type == cmf::ElementType::UInt16Norm ) )
			{
				const std::string name = GenerateAttributeName( prim.attributes, "WEIGHTS_", elem.usageIndex );
				prim.attributes[name] = AddVertexAttribute( vbBytes, vertexCount, lod.vb.stride, elem, gltfBuffer, model, true );
			}
		}

		const int meshIdx = static_cast<int>( model.meshes.size() );
		{
			tinygltf::Mesh gltfMesh;
			gltfMesh.name = meshName;
			gltfMesh.primitives.push_back( prim );
			model.meshes.push_back( gltfMesh );
		}

		const int nodeIdx = static_cast<int>( model.nodes.size() );
		{
			tinygltf::Node node;
			node.name = meshName;
			node.mesh = meshIdx;
			model.nodes.push_back( node );
		}

		lodNodeIndices.push_back( nodeIdx );
	}

	// Attach lower LODs to the LOD0 node via MSFT_lod. Only LOD0 is in the scene
	if( lodNodeIndices.size() > 1 )
	{
		tinygltf::Value::Array ids;
		for( size_t i = 1; i < lodNodeIndices.size(); ++i )
			ids.push_back( tinygltf::Value( lodNodeIndices[i] ) );

		tinygltf::Value::Object msftLod;
		msftLod["ids"] = tinygltf::Value( ids );
		model.nodes[lodNodeIndices[0]].extensions["MSFT_lod"] = tinygltf::Value( msftLod );

		if( std::find( model.extensionsUsed.begin(), model.extensionsUsed.end(), "MSFT_lod" ) == model.extensionsUsed.end() )
			model.extensionsUsed.push_back( "MSFT_lod" );
	}

	scene.nodes.push_back( lodNodeIndices[0] );
}

void GLTFConverter( CLI::App& app, GLTFOptions& options )
{
	app.add_option( "src", options.srcPath, "Path to the source CMF file" )->required()->check( CLI::ExistingFile );
	app.add_option( "dst", options.dstPath, "Path to the output glTF" )->required();
	app.add_flag( "--combinedfile", options.combinedFile, "Should we store the .bin data inside the gltf file as base64" );
	app.final_callback( [&options]() {
		CmfFile cmfFile( options.srcPath );
		auto& data = cmfFile.GetData();
		auto& bufferManager = cmfFile.GetBufferManager();

		tinygltf::Model model;
		tinygltf::TinyGLTF writer;

		tinygltf::Buffer gltfBuffer;
		tinygltf::Scene scene;

		if( data.meshes.empty() )
		{
			fprintf( stderr, "CMF contains no mesh data" );
			return;
		}

		for( const auto& mesh : data.meshes )
		{
			AddMesh( mesh, bufferManager, gltfBuffer, model, scene );
		}

		model.buffers.push_back( gltfBuffer );
		model.scenes.push_back( scene );
		model.defaultScene = 0;
		model.asset.version = "2.0"; // This is the GLTF version we want to use, not our internal asset version
		model.asset.generator = "cmfprocessor";

		const bool ok = writer.WriteGltfSceneToFile( &model, options.dstPath, false, options.combinedFile, true, false );
		if( !ok )
		{
			throw std::runtime_error( "Failed to write glTF: " + options.dstPath );
		}
	} );
}
} // namespace

REGISTER_COMMAND(
	"gltfconverter",
	"Export a CMF file to glTF",
	&GLTFConverter );

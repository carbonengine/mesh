#include "commands.h"
#include "cmffile.h"
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tiny_gltf.h>

#include <cfloat>

namespace
{
struct GLTFOptions
{
	std::string srcPath;
	std::string dstPath;
	bool combinedFile = false;
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

int AddVertexAttributeFloat( const uint8_t* vbBytes, uint32_t vertexCount, uint32_t stride, const cmf::VertexElement& element, tinygltf::Buffer& gltfBuffer, tinygltf::Model& model )
{
	const int componentCount = element.elementCount;

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

	AlignBuffer( gltfBuffer, 4 );
	const size_t byteOffset = gltfBuffer.data.size();
	const size_t byteLength = static_cast<size_t>( vertexCount ) * componentCount * sizeof( float );
	gltfBuffer.data.resize( byteOffset + byteLength );

	std::vector<double> minVals( componentCount, DBL_MAX );
	std::vector<double> maxVals( componentCount, -DBL_MAX );

	float* dst = reinterpret_cast<float*>( gltfBuffer.data.data() + byteOffset );
	for( uint32_t v = 0; v < vertexCount; ++v )
	{
		const float* src = reinterpret_cast<const float*>( vbBytes + v * stride + element.offset );
		for( int k = 0; k < componentCount; ++k )
		{
			dst[v * componentCount + k] = src[k];
			minVals[k] = std::min( minVals[k], static_cast<double>( src[k] ) );
			maxVals[k] = std::max( maxVals[k], static_cast<double>( src[k] ) );
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
		acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
		acc.type = gltfType;
		acc.count = vertexCount;
		acc.minValues = minVals;
		acc.maxValues = maxVals;
		model.accessors.push_back( acc );
	}
	return accIdx;
}

int AddVertexAttributeInteger( const uint8_t* vbBytes, uint32_t vertexCount, uint32_t stride, const cmf::VertexElement& element, tinygltf::Buffer& gltfBuffer, tinygltf::Model& model, bool normalized = false )
{
	const int componentCount = element.elementCount;
	const bool isShort = element.type == cmf::ElementType::UInt16 || element.type == cmf::ElementType::UInt16Norm;
	const size_t componentSize = isShort ? sizeof( uint16_t ) : sizeof( uint8_t );
	const int gltfComponentType = isShort ? TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT : TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;

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

	AlignBuffer( gltfBuffer, static_cast<size_t>( componentSize ) );
	const size_t byteOffset = gltfBuffer.data.size();
	const size_t byteLength = static_cast<size_t>( vertexCount ) * componentCount * componentSize;
	gltfBuffer.data.resize( byteOffset + byteLength );

	uint8_t* dst = gltfBuffer.data.data() + byteOffset;
	for( uint32_t v = 0; v < vertexCount; ++v )
	{
		const uint8_t* src = vbBytes + v * stride + element.offset;
		memcpy( dst + v * componentCount * componentSize, src, componentCount * componentSize );
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
		model.accessors.push_back( acc );
	}
	return accIdx;
}

void AddMesh( const cmf::v1::Mesh& mesh, cmf::BufferManager& bufferManager, tinygltf::Buffer& gltfBuffer, tinygltf::Model& model, tinygltf::Scene& scene )
{
	if( mesh.lods.empty() )
		return;

	const auto* posElement = FindSingleUsageElement( mesh.decl, cmf::Usage::Position );
	if( !posElement || posElement->type != cmf::ElementType::Float32 || posElement->elementCount != 3 )
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
			fprintf( stderr, "Unsupported MeshTopology" );
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

		prim.attributes["POSITION"] = AddVertexAttributeFloat( vbBytes, vertexCount, lod.vb.stride, *posElement, gltfBuffer, model );

		// Single float attributes channels
		static const struct { cmf::Usage usage; const char* name; } kSingleAttribs[] = {
			{ cmf::Usage::Normal,   "NORMAL"    },
			{ cmf::Usage::Tangent,  "TANGENT"   },
			{ cmf::Usage::Binormal, "_BINORMAL" },
		};
		for( const auto& attrib : kSingleAttribs )
		{
			const auto* elem = FindSingleUsageElement( mesh.decl, attrib.usage );
			if( elem && elem->type == cmf::ElementType::Float32 )
				prim.attributes[attrib.name] = AddVertexAttributeFloat( vbBytes, vertexCount, lod.vb.stride, *elem, gltfBuffer, model );
		}

		// Float attributes that can have multiple channels (TEXCOORD_n, COLOR_n)
		static const struct { cmf::Usage usage; const char* prefix; } kMultiAttribs[] = {
			{ cmf::Usage::TexCoord, "TEXCOORD" },
			{ cmf::Usage::Color, "COLOR" },
		};
		for( const auto& attrib : kMultiAttribs )
		{
			for( const auto& elem : mesh.decl )
			{
				if( elem.usage == attrib.usage && elem.type == cmf::ElementType::Float32 )
				{
					const std::string name = std::string( attrib.prefix ) + "_" + std::to_string( elem.usageIndex );
					prim.attributes[name] = AddVertexAttributeFloat( vbBytes, vertexCount, lod.vb.stride, elem, gltfBuffer, model );
				}
			}
		}

		// Integer attributes (JOINTS_n)
		for( const auto& elem : mesh.decl )
		{
			if ( elem.usage == cmf::Usage::BoneIndices &&
			     ( elem.type == cmf::ElementType::UInt8 || elem.type == cmf::ElementType::UInt16 ) )
			{
				const std::string name = std::string( "JOINTS_" ) + std::to_string( elem.usageIndex );
				prim.attributes[name] = AddVertexAttributeInteger( vbBytes, vertexCount, lod.vb.stride, elem, gltfBuffer, model );
			}
		}

		// Normalized integer attributes (WEIGHTS_n for non-float bone weights)
		for( const auto& elem : mesh.decl )
		{
			if( elem.usage == cmf::Usage::BoneWeights &&
				( elem.type == cmf::ElementType::UInt8 || elem.type == cmf::ElementType::UInt8Norm ||
				  elem.type == cmf::ElementType::UInt16 || elem.type == cmf::ElementType::UInt16Norm ) )
			{
				const std::string name = std::string( "WEIGHTS_" ) + std::to_string( elem.usageIndex );
				prim.attributes[name] = AddVertexAttributeInteger( vbBytes, vertexCount, lod.vb.stride, elem, gltfBuffer, model, true );
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
			AddMesh( mesh, bufferManager, gltfBuffer, model, scene );

		model.buffers.push_back( gltfBuffer );
		model.scenes.push_back( scene );
		model.defaultScene = 0;
		model.asset.version = "2.0"; // This is the GLTF version we want to use, not our internal asset version
		model.asset.generator = "cmfprocessor";

		const bool ok = writer.WriteGltfSceneToFile( &model, options.dstPath, false, options.combinedFile, true, false );
		if( !ok )
		{
			fprintf( stderr, "Failed to write glTF: %s\n", options.dstPath.c_str() );
		}
	} );
}
} // namespace

REGISTER_COMMAND(
	"gltfconverter",
	"Export a CMF file to glTF",
	&GLTFConverter );

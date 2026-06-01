#include "commands.h"
#include "cmffile.h"
#include "cmf/tangents.h"
#include "cmf/declutils.h"
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tiny_gltf.h>

#include <cfloat>
#include <stdexcept>
#include <cmf/bufferstreams.h>

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

void AlignBuffer( tinygltf::Buffer& buf, size_t alignment )
{
	while( buf.data.size() % alignment != 0 )
	{
		buf.data.push_back( 0 );
	}
}

int GetGltfComponentType( cmf::ElementType element )
{
	switch( element )
	{
	case cmf::ElementType::Float32:
	// We convert float16 into float32 for gltf compatability
	case cmf::ElementType::Float16:
		return TINYGLTF_COMPONENT_TYPE_FLOAT;
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

std::string GenerateAttributeName( const std::map<std::string, int>& usedAttributeNames, const CMFUsageAttribute& atribute, int usageIndex )
{
	// Continue for a reasonable amount of similarly named attributes.
	// VK and GL guarantee at least 16 elements, so we will provide a worst case senario
	const int minimumGLAttributes = 16;
	for( int i = usageIndex; i < minimumGLAttributes; i++ )
	{
		std::string newName = atribute.prefix;
		if( i > 0 || ( atribute.usage == cmf::Usage::TexCoord || atribute.usage == cmf::Usage::Color || atribute.usage == cmf::Usage::BoneIndices || atribute.usage == cmf::Usage::BoneWeights ) )
		{
			newName += "_" + std::to_string( i );
		}
		if( usedAttributeNames.find( newName ) == usedAttributeNames.end() )
		{
			return newName;
		}
	}
	throw std::runtime_error( "Could not find a unique attribute name for '" + std::string( atribute.prefix ) + "' starting at index " + std::to_string( usageIndex ) );
}

template <typename T>
void StoreVertexData( const uint8_t* vbBytes, const uint32_t vertexCount, uint32_t stride, const cmf::VertexElement& element, float* dest, std::vector<double>& minVals, std::vector<double>& maxVals )
{
	const cmf::ConstBufferElementStream<T> dataStream{ element, vbBytes, vertexCount, stride };
	uint32_t size = dataStream.size();
	for( uint32_t i = 0; i < size; i++ )
	{
		T value = dataStream[i];

		for( int k = 0; k < element.elementCount; ++k )
		{
			dest[i * element.elementCount + k] = value[k];
			minVals[k] = std::min( minVals[k], static_cast<double>( value[k] ) );
			maxVals[k] = std::max( maxVals[k], static_cast<double>( value[k] ) );
		}
	}
}

int AddVertexAttribute( const uint8_t* vbBytes, const uint32_t vertexCount, uint32_t stride, const cmf::VertexElement& element, tinygltf::Buffer& gltfBuffer, tinygltf::Model& model )
{
	const int componentCount = element.elementCount;
	const size_t componentSize = cmf::GetElementTypeSize( element.type );
	const int gltfComponentType = GetGltfComponentType( element.type );
	const bool normalized = cmf::IsNormalizedElementType( element.type );

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

	AlignBuffer( gltfBuffer, sizeof( float ) );
	const size_t byteOffset = gltfBuffer.data.size();
	const size_t byteLength = static_cast<size_t>( vertexCount ) * componentCount * sizeof( float );
	gltfBuffer.data.resize( byteOffset + byteLength );

	// Stores the min/max ranges of the elements components
	std::vector<double> minVals( componentCount, DBL_MAX );
	std::vector<double> maxVals( componentCount, -DBL_MAX );

	uint8_t* dst = gltfBuffer.data.data() + byteOffset;

	auto* floatDst = reinterpret_cast<float*>( dst ); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

	switch( componentCount )
	{
	case 2:
		StoreVertexData<Vector2>( vbBytes, vertexCount, stride, element, floatDst, minVals, maxVals );
		break;
	case 3:
		StoreVertexData<Vector3>( vbBytes, vertexCount, stride, element, floatDst, minVals, maxVals );
		break;
	case 4:
		StoreVertexData<Vector4>( vbBytes, vertexCount, stride, element, floatDst, minVals, maxVals );
		break;
	default:
		const cmf::ConstBufferElementStream<float> dataStream{ element, vbBytes, vertexCount, stride };
		for( uint32_t i = 0; i < dataStream.size(); i++ )
		{
			float value = dataStream[i];
			floatDst[i * element.elementCount] = value;
			minVals[0] = std::min( minVals[0], static_cast<double>( value ) );
			maxVals[0] = std::max( maxVals[0], static_cast<double>( value ) );
		}
		break;
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
		acc.minValues = minVals;
		acc.maxValues = maxVals;
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

int AddTangentAttribute( const uint8_t* vbBytes, const uint32_t vertexCount, uint32_t stride, const cmf::VertexElement& tangentElem, cmf::Span<cmf::VertexElement> decl, tinygltf::Buffer& gltfBuffer, tinygltf::Model& model )
{
	const auto* normalElem = FindElement( decl, cmf::Usage::Normal, tangentElem.usageIndex );
	const auto* binormalElem = FindElement( decl, cmf::Usage::Binormal, tangentElem.usageIndex );

	if( normalElem == nullptr || normalElem == nullptr )
	{
		throw std::runtime_error( "Unable to find matching T,B,N data set with same usage index of " + tangentElem.usageIndex );
		return -1;
	}

	// Rebuild the tangent buffer storing the binormal in the W component according to the spec under "TANGENT"
	// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#meshes-overview
	std::vector<float> tanBuffer( vertexCount * 4 );
	for( uint32_t v = 0; v < vertexCount; ++v )
	{
		const cmf::ConstBufferElementStream<Vector3> t{ tangentElem, vbBytes, vertexCount, stride };
		const cmf::ConstBufferElementStream<Vector3> n{ *normalElem, vbBytes, vertexCount, stride };
		const cmf::ConstBufferElementStream<Vector3> b{ *binormalElem, vbBytes, vertexCount, stride };
		const float w = GenerateBinormalSign( n[0], t[0], b[0] );
		tanBuffer[v * 4 + 0] = t[0].x;
		tanBuffer[v * 4 + 1] = t[0].y;
		tanBuffer[v * 4 + 2] = t[0].z;
		tanBuffer[v * 4 + 3] = w;
	}

	cmf::VertexElement tanElem{};
	tanElem.usage = cmf::Usage::Tangent;
	tanElem.type = cmf::ElementType::Float32;
	tanElem.elementCount = 4;
	tanElem.offset = 0;

	return AddVertexAttribute( reinterpret_cast<const uint8_t*>( tanBuffer.data() ), vertexCount, 4 * sizeof( float ), tanElem, gltfBuffer, model ); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

std::pair<int, int> AddPackedTangentAttribute( const uint8_t* vbBytes, const uint32_t vertexCount, const uint32_t stride, const cmf::VertexElement& element, tinygltf::Buffer& gltfBuffer, tinygltf::Model& model )
{
	const bool isQuaternion = element.usage == cmf::Usage::PackedTangent;
	const cmf::TangentCompression compression = isQuaternion ? cmf::TangentCompression::PackedTangent : cmf::TangentCompression::PackedTangentLegacy;

	std::vector<float> normalBuffer( vertexCount * 3 );
	std::vector<float> tangentBuffer( vertexCount * 4 );

	for( uint32_t v = 0; v < vertexCount; ++v )
	{
		const uint8_t* src = vbBytes + v * stride + element.offset;

		const cmf::ConstBufferElementStream<Vector4> elementStreamTangent{ element, vbBytes, vertexCount, stride };

		const Vector4 packed = elementStreamTangent[0];

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
	normalElem.usage = cmf::Usage::Normal;
	normalElem.type = cmf::ElementType::Float32;
	normalElem.elementCount = 3;
	normalElem.offset = 0;

	cmf::VertexElement tanElem{};
	tanElem.usage = cmf::Usage::Tangent;
	tanElem.type = cmf::ElementType::Float32;
	tanElem.elementCount = 4;
	tanElem.offset = 0;

	const int normalAccIdx = AddVertexAttribute( reinterpret_cast<const uint8_t*>( normalBuffer.data() ), vertexCount, 3 * sizeof( float ), normalElem, gltfBuffer, model ); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
	const int tangentAccIdx = AddVertexAttribute( reinterpret_cast<const uint8_t*>( tangentBuffer.data() ), vertexCount, 4 * sizeof( float ), tanElem, gltfBuffer, model ); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

	return { normalAccIdx, tangentAccIdx };
}

void AddMesh( const cmf::Mesh& mesh, cmf::BufferManager& bufferManager, tinygltf::Buffer& gltfBuffer, tinygltf::Model& model, tinygltf::Scene& scene )
{
	if( mesh.lods.empty() )
	{
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
		case cmf::MeshTopology::PointList: {
			prim.mode = TINYGLTF_MODE_POINTS;
			break;
		}
		case cmf::MeshTopology::TriangleList: {
			prim.mode = TINYGLTF_MODE_TRIANGLES;
			break;
		}
		default: {
			printf( "Unsupported MeshTopology\n" );
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
			{ cmf::Usage::BoneIndices, "JOINTS" },
			{ cmf::Usage::BoneWeights, "WEIGHTS" },
		};
		for( const auto& attrib : kMultiAttribs )
		{
			for( const auto& elem : mesh.decl )
			{
				if( elem.usage == attrib.usage && attrib.usage == cmf::Usage::Tangent )
				{
					std::string tangentName = GenerateAttributeName( prim.attributes, attrib, elem.usageIndex );
					prim.attributes[tangentName] = AddTangentAttribute( vbBytes, vertexCount, lod.vb.stride, elem, mesh.decl, gltfBuffer, model );
				}
				else if( elem.usage == attrib.usage && ( attrib.usage == cmf::Usage::PackedTangent || attrib.usage == cmf::Usage::PackedTangentLegacy ) )
				{
					auto [normalAccIdx, tangentAccIdx] = AddPackedTangentAttribute( vbBytes, vertexCount, lod.vb.stride, elem, gltfBuffer, model );

					std::string normalName = GenerateAttributeName( prim.attributes, { cmf::Usage::Normal, "NORMAL" }, elem.usageIndex );
					std::string tangentName = GenerateAttributeName( prim.attributes, { cmf::Usage::Tangent, "TANGENT" }, elem.usageIndex );
					prim.attributes[normalName] = normalAccIdx;
					prim.attributes[tangentName] = tangentAccIdx;
				}
				else if( elem.usage == attrib.usage )
				{
					std::string name = GenerateAttributeName( prim.attributes, attrib, elem.usageIndex );

					prim.attributes[name] = AddVertexAttribute( vbBytes, vertexCount, lod.vb.stride, elem, gltfBuffer, model );
				}
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
		tinygltf::Value::Array ids{};
		for( size_t i = 1; i < lodNodeIndices.size(); ++i )
		{
			ids.push_back( tinygltf::Value( lodNodeIndices[i] ) );
		}

		tinygltf::Value::Object msftLod{};
		msftLod["ids"] = tinygltf::Value( ids );
		model.nodes[lodNodeIndices[0]].extensions["MSFT_lod"] = tinygltf::Value( msftLod );

		if( std::find( model.extensionsUsed.begin(), model.extensionsUsed.end(), "MSFT_lod" ) == model.extensionsUsed.end() )
		{
			model.extensionsUsed.push_back( "MSFT_lod" );
		}
	}

	scene.nodes.push_back( lodNodeIndices[0] );
}

void AddSkeleton( const cmf::Skeleton& skeleton, tinygltf::Buffer& gltfBuffer, tinygltf::Model& model, tinygltf::Scene& scene )
{
	const auto boneCount = static_cast<uint32_t>( skeleton.bones.size() );

	if( boneCount == 0 )
	{
		return;
	}

	const std::string skelName( skeleton.name.begin(), skeleton.name.end() );

	const auto boneNodeOffset = model.nodes.size();

	for( uint32_t i = 0; i < boneCount; ++i )
	{
		const std::string boneName( skeleton.bones[i].begin(), skeleton.bones[i].end() );
		const cmf::Transform& t = skeleton.restTransforms[i];

		tinygltf::Node node;
		node.name = boneName;
		node.translation = { t.position.x, t.position.y, t.position.z };
		node.rotation = { t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w };
		node.scale = { t.scale.x, t.scale.y, t.scale.z };

		model.nodes.push_back( node );
	}

	// Connect the bone parent-child hierarchy. root bones are stored with parent == UINT32_MAX
	int rootNodeIdx = -1;
	for( uint32_t i = 0; i < boneCount; ++i )
	{
		const uint32_t parentIdx = skeleton.parents[i];
		if( parentIdx >= boneCount )
		{
			rootNodeIdx = static_cast<int>( boneNodeOffset + i );
		}
		else
		{
			model.nodes[static_cast<int>( boneNodeOffset + parentIdx )].children.push_back( static_cast<int>( boneNodeOffset + i ) );
		}
	}

	// Write inverse bind matrices as column-major
	AlignBuffer( gltfBuffer, sizeof( float ) );
	const size_t bindTransformByteOffset = gltfBuffer.data.size();
	gltfBuffer.data.resize( bindTransformByteOffset + boneCount * sizeof( Matrix ) );

	uint8_t* dst = gltfBuffer.data.data() + bindTransformByteOffset;

	memcpy( dst, skeleton.invBindTransforms.data(), sizeof( Matrix ) * boneCount );

	const int bindTransformBVIdx = static_cast<int>( model.bufferViews.size() );
	{
		tinygltf::BufferView bv;
		bv.buffer = 0;
		bv.byteOffset = bindTransformByteOffset;
		bv.byteLength = boneCount * sizeof( Matrix );
		model.bufferViews.push_back( bv );
	}

	const int bindTransformIdx = static_cast<int>( model.accessors.size() );
	{
		tinygltf::Accessor acc;
		acc.bufferView = bindTransformBVIdx;
		acc.byteOffset = 0;
		acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
		acc.type = TINYGLTF_TYPE_MAT4;
		acc.count = boneCount;
		model.accessors.push_back( acc );
	}

	tinygltf::Skin skin;
	skin.name = skelName;
	skin.skeleton = rootNodeIdx;
	skin.inverseBindMatrices = bindTransformIdx;
	for( uint32_t i = 0; i < boneCount; ++i )
	{
		skin.joints.push_back( static_cast<int>( boneNodeOffset + i ) );
	}
	model.skins.push_back( skin );

	if( rootNodeIdx >= 0 )
	{
		scene.nodes.push_back( rootNodeIdx );
	}
}

void AddAnimation( const cmf::Animation& animation, tinygltf::Buffer& gltfBuffer, tinygltf::Model& model )
{
	if( animation.channels.empty() )
	{
		return;
	}

	const std::string animName( animation.name.begin(), animation.name.end() );

	tinygltf::Animation gltfAnim;
	gltfAnim.name = animName;

	for( const auto& channel : animation.channels )
	{
		if( channel.curveIndex >= animation.curves.size() )
		{
			continue;
		}

		const cmf::AnimationCurve& curve = animation.curves[channel.curveIndex];

		std::string path;
		switch( channel.targetType )
		{
		case cmf::AnimationChannelTargetType::BonePosition:
			path = "translation";
			break;
		case cmf::AnimationChannelTargetType::BoneRotation:
			path = "rotation";
			break;
		case cmf::AnimationChannelTargetType::BoneScale:
			path = "scale";
			break;
		case cmf::AnimationChannelTargetType::MorphTarget:
			path = "weights";
			break;
		default:
			continue;
		}

		int gltfValueType;
		switch( curve.valueDimension )
		{
		case 1:
			gltfValueType = TINYGLTF_TYPE_SCALAR;
			break;
		case 3:
			gltfValueType = TINYGLTF_TYPE_VEC3;
			break;
		case 4:
			gltfValueType = TINYGLTF_TYPE_VEC4;
			break;
		default:
			continue;
		}

		// Find the target bone node by name
		const std::string targetName( channel.target.begin(), channel.target.end() );
		int targetNodeIdx = -1;
		for( int i = 0; i < static_cast<int>( model.nodes.size() ); ++i )
		{
			if( model.nodes[i].name == targetName )
			{
				targetNodeIdx = i;
				break;
			}
		}
		if( targetNodeIdx < 0 )
		{
			continue;
		}

		const std::string interpolation = curve.interpolation == cmf::Interpolation::Linear ? "LINEAR" : "STEP";

		cmf::VertexElement knotElement = {};
		knotElement.type = curve.knotType;
		knotElement.elementCount = 1;

		const auto knotStride = cmf::GetVertexElementSize( knotElement );
		const cmf::ConstBufferElementStream<float> knotFloats{ knotElement, curve.knots.data(), curve.knotCount, knotStride };

		AlignBuffer( gltfBuffer, sizeof( float ) );
		const size_t knotsByteOffset = gltfBuffer.data.size();
		const size_t knotsByteLength = curve.knotCount * sizeof( float );
		gltfBuffer.data.resize( knotsByteOffset + knotsByteLength );

		// gltf requires min/max on the time accessor
		double knotMin = DBL_MAX;
		double knotMax = -DBL_MAX;

		for( uint32_t i = 0; i < curve.knotCount; i++ )
		{
			float value = knotFloats[i];
			knotMin = std::min( knotMin, static_cast<double>( value ) );
			knotMax = std::max( knotMax, static_cast<double>( value ) );
			memcpy( gltfBuffer.data.data() + knotsByteOffset + i * sizeof( float ), &value, sizeof( float ) );
		}

		const int inputBVIdx = static_cast<int>( model.bufferViews.size() );
		{
			tinygltf::BufferView bv;
			bv.buffer = 0;
			bv.byteOffset = knotsByteOffset;
			bv.byteLength = knotsByteLength;
			model.bufferViews.push_back( bv );
		}

		const int inputAccIdx = static_cast<int>( model.accessors.size() );
		{
			tinygltf::Accessor acc;
			acc.bufferView = inputBVIdx;
			acc.byteOffset = 0;
			acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
			acc.type = TINYGLTF_TYPE_SCALAR;
			acc.count = curve.knotCount;
			acc.minValues = { knotMin };
			acc.maxValues = { knotMax };
			model.accessors.push_back( acc );
		}

		cmf::VertexElement valueElement = {};
		valueElement.type = curve.valueType;
		valueElement.elementCount = curve.valueDimension;
		const auto valueStride = cmf::GetVertexElementSize( valueElement );
		const cmf::ConstBufferElementStream<float> valueFloats{ valueElement, curve.values.data(), uint32_t( curve.values.size() / valueStride ), valueStride };

		// Write animation values
		AlignBuffer( gltfBuffer, sizeof( float ) );
		size_t valuesByteOffset = gltfBuffer.data.size();
		const size_t valuesByteLength = curve.knotCount * curve.valueDimension * sizeof( float );
		gltfBuffer.data.resize( valuesByteOffset + valuesByteLength );

		for( uint32_t i = 0; i < valueFloats.size(); i++ )
		{
			float value = valueFloats[i];
			memcpy( gltfBuffer.data.data() + valuesByteOffset + i * sizeof( float ), &value, sizeof( float ) );
		}

		const int outputBVIdx = static_cast<int>( model.bufferViews.size() );
		{
			tinygltf::BufferView bv;
			bv.buffer = 0;
			bv.byteOffset = valuesByteOffset;
			bv.byteLength = valuesByteLength;
			model.bufferViews.push_back( bv );
		}

		const int outputAccIdx = static_cast<int>( model.accessors.size() );
		{
			tinygltf::Accessor acc;
			acc.bufferView = outputBVIdx;
			acc.byteOffset = 0;
			acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
			acc.type = gltfValueType;
			acc.count = curve.knotCount;
			model.accessors.push_back( acc );
		}

		const int samplerIdx = static_cast<int>( gltfAnim.samplers.size() );
		{
			tinygltf::AnimationSampler sampler;
			sampler.input = inputAccIdx;
			sampler.output = outputAccIdx;
			sampler.interpolation = interpolation;
			gltfAnim.samplers.push_back( sampler );
		}

		{
			tinygltf::AnimationChannel gltfChannel;
			gltfChannel.sampler = samplerIdx;
			gltfChannel.target_node = targetNodeIdx;
			gltfChannel.target_path = path;
			gltfAnim.channels.push_back( gltfChannel );
		}
	}

	if( !gltfAnim.channels.empty() )
	{
		model.animations.push_back( gltfAnim );
	}
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

		for( const auto& mesh : data.meshes )
		{
			AddMesh( mesh, bufferManager, gltfBuffer, model, scene );
		}

		for( const auto& skeleton : data.skeletons )
		{
			AddSkeleton( skeleton, gltfBuffer, model, scene );
		}

		for( const auto& animation : data.animations )
		{
			AddAnimation( animation, gltfBuffer, model );
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

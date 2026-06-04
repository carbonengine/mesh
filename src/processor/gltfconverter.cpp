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
#include <cmf/bufferutils.h>

namespace
{
struct GLTFOptions
{
	std::string srcPath;
	std::string dstPath;
	bool combinedFile = false;
};

const char* GetGltfAttributeName( cmf::Usage usage )
{
	switch( usage )
	{
	case cmf::Usage::Position:
		return "POSITION";
	case cmf::Usage::Normal:
		return "NORMAL";
	case cmf::Usage::Tangent:
		return "TANGENT";
	case cmf::Usage::TexCoord:
		return "TEXCOORD";
	case cmf::Usage::Color:
		return "COLOR";
	case cmf::Usage::BoneIndices:
		return "JOINTS";
	case cmf::Usage::BoneWeights:
		return "WEIGHTS";
	// Packed tangents are decompressed into normals and tangents, and binormals are baked into the
	// tangent sign according to the glTF spec, so they have no attribute name of their own
	case cmf::Usage::PackedTangent:
	case cmf::Usage::PackedTangentLegacy:
	case cmf::Usage::Binormal:
		return "";
	}
	return "";
}

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
		return TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
	case cmf::ElementType::Int16Norm:
	case cmf::ElementType::Int16:
		return TINYGLTF_COMPONENT_TYPE_SHORT;
	case cmf::ElementType::UInt8Norm:
	case cmf::ElementType::UInt8:
		return TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
	case cmf::ElementType::Int8Norm:
	case cmf::ElementType::Int8:
		return TINYGLTF_COMPONENT_TYPE_BYTE;
	}
	throw std::runtime_error( "Unsupported element type for glTF export" );
}

std::string GenerateAttributeName( const std::map<std::string, int>& usedAttributeNames, const cmf::Usage usage, int usageIndex )
{
	// Continue for a reasonable amount of similarly named attributes.
	// VK and GL guarantee at least 16 elements, so we will provide a worst case senario
	const int minimumGLAttributes = 16;
	std::string atributeName = GetGltfAttributeName( usage );
	for( int i = usageIndex; i < minimumGLAttributes; i++ )
	{
		std::string newName = atributeName;
		if( i > 0 || ( usage == cmf::Usage::TexCoord || usage == cmf::Usage::Color || usage == cmf::Usage::BoneIndices || usage == cmf::Usage::BoneWeights ) )
		{
			newName += "_" + std::to_string( i );
		}
		if( usedAttributeNames.find( newName ) == usedAttributeNames.end() )
		{
			return newName;
		}
	}
	throw std::runtime_error( "Could not find a unique attribute name for '" + atributeName + "' starting at index " + std::to_string( usageIndex ) );
}

int GetGLTFTypeFromComponentCount( int componentCount )
{
	switch( componentCount )
	{
	case 2:
		return TINYGLTF_TYPE_VEC2;
	case 3:
		return TINYGLTF_TYPE_VEC3;
	case 4:
		return TINYGLTF_TYPE_VEC4;
	default:
		return TINYGLTF_TYPE_SCALAR;
	}
}

float GenerateBinormalSign( const Vector3& normal, const Vector3& tangent, const Vector3& bitangent )
{
	const float cx = normal.y * tangent.z - normal.z * tangent.y;
	const float cy = normal.z * tangent.x - normal.x * tangent.z;
	const float cz = normal.x * tangent.y - normal.y * tangent.x;
	return ( cx * bitangent.x + cy * bitangent.y + cz * bitangent.z ) < 0.0f ? -1.0f : 1.0f;
}

void AddSkeletons( CmfFile& cmfFile, tinygltf::Buffer& gltfBuffer, tinygltf::Model& model, tinygltf::Scene& scene )
{
	auto& data = cmfFile.GetData();

	for( const auto& skeleton : data.skeletons )
	{
		const auto boneCount = static_cast<uint32_t>( skeleton.bones.size() );

		if( boneCount == 0 )
		{
			continue;
		}

		const std::string skelName( skeleton.name.begin(), skeleton.name.end() );

		const auto boneNodeOffset = model.nodes.size();

		uint32_t i = 0;
		for( const auto& bone : skeleton.bones )
		{
			const std::string boneName( bone.begin(), bone.end() );
			const cmf::Transform& t = skeleton.restTransforms[i++];

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
}

void AddAnimations( CmfFile& cmfFile, tinygltf::Buffer& gltfBuffer, tinygltf::Model& model )
{
	auto& data = cmfFile.GetData();

	for( const auto& animation : data.animations )
	{
		if( animation.channels.empty() )
		{
			continue;
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
			case cmf::AnimationChannelTargetType::Other:
				printf( "Unsupported AnimationChannelTargetType, continuing.\n" );
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
			valueElement.elementCount = 1; //curve.valueDimension;
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
}

void PreprocessCmfFile( CmfFile& cmfFile )
{
	auto& data = cmfFile.GetData();
	auto& bufferManager = cmfFile.GetBufferManager();
	auto& allocator = cmfFile.GetAllocator();

	// Pre process the cmf file and unpack the tangents into the t, b, n datasets
	for( auto& mesh : data.meshes )
	{
		std::vector<uint32_t> packedTangents;
		for( const auto& elem : mesh.decl )
		{
			if( elem.usage == cmf::Usage::PackedTangent || elem.usage == cmf::Usage::PackedTangentLegacy )
			{
				packedTangents.push_back( elem.usageIndex );
			}
		}
		for( const auto usageIndex : packedTangents )
		{
			cmf::DecompressTangents( mesh, usageIndex, cmfFile.GetAllocator(), cmfFile.GetBufferManager() );
		}
	}

	// Generate new decl for the mesh where all data is stored in float format
	for( auto& mesh : data.meshes )
	{
		cmf::Span<cmf::VertexElement> newDecl;
		for( const auto& elem : mesh.decl )
		{
			if( elem.usage == cmf::Usage::Binormal )
			{
				// Skip binormal elements since we are converting them into tangents with binormal sign baked in according to the glTF spec
			}
			else if( elem.usage == cmf::Usage::Tangent )
			{
				// glTF requires TANGENT to be a float VEC4 with the binormal sign in the W component
				auto newElem = elem;
				newElem.type = cmf::ElementType::Float32;
				newElem.elementCount = 4;
				cmf::Modify( newDecl, allocator ).push_back( newElem );
			}
			else if( elem.usage == cmf::Usage::BoneIndices )
			{
				// glTF requires JOINTS to be unsigned byte/short, never float or signed
				auto newElem = elem;
				if( elem.type != cmf::ElementType::UInt8 && elem.type != cmf::ElementType::UInt16 )
				{
					// Convert any other source type to UInt16, which is large enough for any bone count we support
					newElem.type = cmf::ElementType::UInt16;
				}
				cmf::Modify( newDecl, allocator ).push_back( newElem );
			}
			else if( elem.usage == cmf::Usage::Position || elem.usage == cmf::Usage::Normal || elem.type == cmf::ElementType::Float16 )
			{
				// glTF requires POSITION and NORMAL to be float, and we convert float16 into float32 for compatability
				auto newElem = elem;
				newElem.type = cmf::ElementType::Float32;
				cmf::Modify( newDecl, allocator ).push_back( newElem );
			}
			else
			{
				cmf::Modify( newDecl, allocator ).push_back( elem );
			}
		}
		uint32_t offset = 0;
		for( auto& elem : newDecl )
		{
			elem.offset = offset;
			offset += cmf::GetVertexElementSize( elem );
		}
		for( auto& lod : mesh.lods )
		{
			auto vb = cmf::ChangeBufferVertexDeclaration( lod.vb, mesh.decl, newDecl, allocator, bufferManager );
			for( const auto& elem : newDecl )
			{
				if( elem.usage == cmf::Usage::Tangent )
				{
					const auto* binormalElem = cmf::FindElement( mesh.decl, cmf::Usage::Binormal, elem.usageIndex );
					auto* normalElem = cmf::FindElement( mesh.decl, cmf::Usage::Normal, elem.usageIndex );
					if( !normalElem )
					{
						normalElem = cmf::FindElement( mesh.decl, cmf::Usage::Normal );
					}
					if( binormalElem && normalElem )
					{
						const cmf::BufferElementStream<Vector4> tangents( elem, vb, bufferManager );
						const cmf::ConstBufferElementStream<Vector3> normals( *normalElem, lod.vb, bufferManager );
						const cmf::ConstBufferElementStream<Vector3> binormals( *binormalElem, lod.vb, bufferManager );
						for( uint32_t i = 0; i < tangents.size(); ++i )
						{
							auto tangent = tangents[i].GetXYZ();
							const float w = GenerateBinormalSign( normals[i], tangent, binormals[i] );
							tangents.set( i, Vector4{ tangent, w } );
						}
					}
					else
					{
						// No binormal/normal pair to derive the sign from, default W to +1 since glTF requires W = +-1
						const cmf::BufferElementStream<Vector4> tangents( elem, vb, bufferManager );
						for( uint32_t i = 0; i < tangents.size(); ++i )
						{
							tangents.set( i, Vector4{ tangents[i].GetXYZ(), 1.0f } );
						}
					}
				}
			}
			lod.vb = vb;
		}
		mesh.decl = newDecl;
	}
}

void AddMeshes( CmfFile& cmfFile, tinygltf::Buffer& gltfBuffer, tinygltf::Model& model, tinygltf::Scene& scene )
{
	auto& data = cmfFile.GetData();
	auto& bufferManager = cmfFile.GetBufferManager();
	auto& allocator = cmfFile.GetAllocator();

	for( const auto& mesh : data.meshes )
	{
		const std::string meshName( mesh.name.begin(), mesh.name.end() );

		std::vector<int> lodNodeIndices;

		for( const auto& lod : mesh.lods )
		{
			const uint32_t vertexCount = lod.vb.stride > 0 ? lod.vb.size / lod.vb.stride : 0;
			if( vertexCount == 0 )
			{
				// glTF does not allow accessors with a count of 0
				continue;
			}

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
				throw std::runtime_error( "Unsupported MeshTopology" );
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

				int indexComponentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
				if( lod.ib.stride == 2 )
				{
					indexComponentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
				}
				else if( lod.ib.stride == 1 )
				{
					indexComponentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
				}

				const int indexAccIdx = static_cast<int>( model.accessors.size() );
				{
					tinygltf::Accessor acc;
					acc.bufferView = indexBVIdx;
					acc.byteOffset = 0;
					acc.componentType = indexComponentType;
					acc.type = TINYGLTF_TYPE_SCALAR;
					acc.count = indexCount;
					model.accessors.push_back( acc );
				}

				prim.indices = indexAccIdx;
			}

			// Process vertex data
			const auto* vbBytes = static_cast<const uint8_t*>( bufferManager.GetData( lod.vb ) );

			AlignBuffer( gltfBuffer, sizeof( float ) );
			const size_t byteOffset = gltfBuffer.data.size();
			const size_t byteLength = static_cast<size_t>( lod.vb.size );
			gltfBuffer.data.resize( byteOffset + byteLength );

			uint8_t* dst = gltfBuffer.data.data() + byteOffset;

			// Since Gltf supports interleaved data, just copy the whole buffer into the new gltf buffer
			memcpy( dst, vbBytes, lod.vb.size );

			// A single interleaved buffer view shared by all the vertex attribute accessors
			const int vbBVIdx = static_cast<int>( model.bufferViews.size() );
			{
				tinygltf::BufferView bv;
				bv.buffer = 0;
				bv.byteOffset = byteOffset;
				bv.byteLength = byteLength;
				bv.target = TINYGLTF_TARGET_ARRAY_BUFFER;
				bv.byteStride = lod.vb.stride;
				model.bufferViews.push_back( bv );
			}

			for( const auto& elem : mesh.decl )
			{
				const std::string name = GenerateAttributeName( prim.attributes, elem.usage, elem.usageIndex );

				tinygltf::Accessor acc;
				acc.bufferView = vbBVIdx;
				acc.byteOffset = elem.offset;
				acc.componentType = GetGltfComponentType( elem.type );
				acc.normalized = cmf::IsNormalizedElementType( elem.type );
				acc.type = GetGLTFTypeFromComponentCount( elem.elementCount );
				acc.count = vertexCount;

				// glTF only requires min/max bounds on the position accessor, but we provide them for all
				// float attributes. Non-float attributes are skipped since this loop reads float components.
				if( elem.type == cmf::ElementType::Float32 )
				{
					std::vector<double> minVals( elem.elementCount, DBL_MAX );
					std::vector<double> maxVals( elem.elementCount, -DBL_MAX );

					for( uint32_t i = 0; i < vertexCount; i++ )
					{
						const uint32_t vertexRowOffset = i * lod.vb.stride + elem.offset;
						for( int j = 0; j < elem.elementCount; j++ )
						{
							float value = 0;
							memcpy( &value, vbBytes + vertexRowOffset + j * sizeof( float ), sizeof( float ) );
							minVals[j] = std::min( minVals[j], static_cast<double>( value ) );
							maxVals[j] = std::max( maxVals[j], static_cast<double>( value ) );
						}
					}

					acc.minValues = minVals;
					acc.maxValues = maxVals;
				}

				const int accIdx = static_cast<int>( model.accessors.size() );
				model.accessors.push_back( acc );
				prim.attributes[name] = accIdx;
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

		if( lodNodeIndices.empty() )
		{
			continue;
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
}

void GLTFConverter( CLI::App& app, GLTFOptions& options )
{
	app.add_option( "src", options.srcPath, "Path to the source CMF file" )->required()->check( CLI::ExistingFile );
	app.add_option( "dst", options.dstPath, "Path to the output glTF" )->required();
	app.add_flag( "--combinedfile", options.combinedFile, "Should we store the .bin data inside the gltf file as base64" );
	app.final_callback( [&options]() {
		CmfFile cmfFile( options.srcPath );

		tinygltf::Model model;
		tinygltf::TinyGLTF writer;

		tinygltf::Buffer gltfBuffer;
		tinygltf::Scene scene;

		PreprocessCmfFile( cmfFile );

		AddMeshes( cmfFile, gltfBuffer, model, scene );
		AddSkeletons( cmfFile, gltfBuffer, model, scene );
		AddAnimations( cmfFile, gltfBuffer, model );

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

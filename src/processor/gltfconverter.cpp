// Copyright © 2026 CCP ehf.

#include "commands.h"
#include "cmffile.h"
#include "cmf/tangents.h"
#include "cmf/declutils.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tiny_gltf.h>

#include <cmf/bufferstreams.h>
#include <cmf/bufferutils.h>
#include <cmf/animation.h>

namespace
{
constexpr uint32_t MAX_SCREEN_SIZE = 2048; // same as lodsimplygon.cpp

struct GLTFOptions
{
	std::string srcPath;
	std::vector<std::string> srcPaths2;
	std::string dstPath;
	bool combinedFile = false;
};

struct MorphMeshNode
{
	int nodeIndex;
	cmf::Span<cmf::MorphTarget> targets;
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
	// We convert float16 into float32 for gltf compatibility
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
	// Continue for a reasonable number of similarly named attributes.
	// Vulkan/OpenGL guarantee at least 16 vertex attributes, so we use that as a worst case.
	int minimumGLAttributes = 16;
	std::string attributeName = GetGltfAttributeName( usage );
	for( int i = usageIndex; i < minimumGLAttributes; i++ )
	{
		std::string newName = attributeName;
		if( usage == cmf::Usage::TexCoord || usage == cmf::Usage::Color || usage == cmf::Usage::BoneIndices || usage == cmf::Usage::BoneWeights )
		{
			newName += "_" + std::to_string( i );
		}
		else if( i > 0 )
		{
			newName = "_" + newName + "_" + std::to_string( i );
		}
		if( usedAttributeNames.find( newName ) == usedAttributeNames.end() )
		{
			return newName;
		}
	}
	throw std::runtime_error( "Could not find a unique attribute name for '" + attributeName + "' starting at index " + std::to_string( usageIndex ) );
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

int ComponentsInType( int gltfType )
{
	switch( gltfType )
	{
	case TINYGLTF_TYPE_SCALAR:
		return 1;
	case TINYGLTF_TYPE_VEC2:
		return 2;
	case TINYGLTF_TYPE_VEC3:
		return 3;
	case TINYGLTF_TYPE_VEC4:
		return 4;
	case TINYGLTF_TYPE_MAT4:
		return 16;
	default:
		return 1;
	}
}

float GenerateBinormalSign( const Vector3& normal, const Vector3& tangent, const Vector3& bitangent )
{
	return Dot( Cross( normal, tangent ), bitangent ) < 0.0f ? -1.0f : 1.0f;
}

template <class T>
int AppendFloatAccessor( tinygltf::Buffer& buffer, tinygltf::Model& model, T& data, int gltfType, bool withMinMax )
{
	int components = ComponentsInType( gltfType );

	AlignBuffer( buffer, sizeof( float ) );
	size_t byteOffset = buffer.data.size();
	size_t byteLength = data.size() * sizeof( float );
	buffer.data.resize( byteOffset + byteLength );

	for( uint32_t i = 0; i < (uint32_t)data.size(); i++ )
	{
		float value = data[i];
		memcpy( buffer.data.data() + byteOffset + i * sizeof( float ), &value, sizeof( float ) );
	}

	int bufferViewIndex = static_cast<int>( model.bufferViews.size() );
	{
		tinygltf::BufferView bufferView;
		bufferView.buffer = 0;
		bufferView.byteOffset = byteOffset;
		bufferView.byteLength = byteLength;
		model.bufferViews.push_back( bufferView );
	}

	tinygltf::Accessor accessor;
	accessor.bufferView = bufferViewIndex;
	accessor.byteOffset = 0;
	accessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
	accessor.type = gltfType;
	accessor.count = static_cast<int>( data.size() / components );

	if( withMinMax && data.size() != 0 )
	{
		std::vector<double> min( components, DBL_MAX );
		std::vector<double> max( components, -DBL_MAX );
		for( uint32_t i = 0; i < (uint32_t)data.size(); i++ )
		{
			int j = static_cast<int>( i % components );
			min[j] = std::min( min[j], static_cast<double>( data[i] ) );
			max[j] = std::max( max[j], static_cast<double>( data[i] ) );
		}
		accessor.minValues = min;
		accessor.maxValues = max;
	}

	int accessorIndex = static_cast<int>( model.accessors.size() );
	model.accessors.push_back( accessor );
	return accessorIndex;
}

std::vector<int32_t> CreateMapping( const cmf::Skeleton& skeleton, cmf::Span<cmf::BoneBinding> boneBindings )
{
	std::vector<int32_t> mapping( boneBindings.size(), -1 );
	for( uint32_t meshBoneIndex = 0; meshBoneIndex < boneBindings.size(); meshBoneIndex++ )
	{
		const auto boundBoneName = boneBindings[meshBoneIndex].name;

		auto foundBone = std::find_if( skeleton.bones.begin(), skeleton.bones.end(), [boundBoneName]( cmf::String boneName ) {
			return boundBoneName == boneName;
		} );
		if( foundBone != skeleton.bones.end() )
		{
			mapping[meshBoneIndex] = (int32_t)std::distance( skeleton.bones.begin(), foundBone );
		}
	}
	return mapping;
}

struct SkeletonNodes
{
	int boneNodeOffset = -1;
	int rootNodeIndex = -1;
};

Quaternion NormalizedQuaternion( Quaternion quaternion )
{
	float squaredLength = LengthSq( quaternion );
	if( squaredLength < FLT_EPSILON )
	{
		return { 0.0, 0.0, 0.0, 1.0 };
	}
	if( std::fabs( squaredLength - 1.0f ) > FLT_EPSILON )
	{
		return Normalize( quaternion );
	}
	return quaternion;
}

std::vector<float> NormalizedQuaternionStream( const std::vector<float>& values )
{
	std::vector<float> result;
	result.resize( values.size() );
	for( size_t i = 0; i < result.size(); i += 4 )
	{
		Quaternion normalized = NormalizedQuaternion( { values[i], values[i + 1], values[i + 2], values[i + 3] } );
		result[i + 0] = normalized.x;
		result[i + 1] = normalized.y;
		result[i + 2] = normalized.z;
		result[i + 3] = normalized.w;
	}
	return result;
}

std::vector<SkeletonNodes> AddSkeletons( CmfFile& cmfFile, tinygltf::Model& model, tinygltf::Scene& scene )
{
	const auto& data = cmfFile.GetData();
	std::vector<SkeletonNodes> result( data.skeletons.size() );

	for( size_t skeletonIndex = 0; skeletonIndex < data.skeletons.size(); skeletonIndex++ )
	{
		const auto& skeleton = data.skeletons[skeletonIndex];
		if( skeleton.bones.size() == 0 )
		{
			continue;
		}

		int boneNodeOffset = (int)model.nodes.size();

		for( size_t boneIndex = 0; boneIndex < skeleton.bones.size(); boneIndex++ )
		{
			const auto& boneName = skeleton.bones[boneIndex];
			const cmf::Transform& transform = skeleton.restTransforms[boneIndex];
			tinygltf::Node node;
			node.name = cmf::ToStdString( boneName );
			node.translation = { transform.position.x, transform.position.y, transform.position.z };
			Quaternion rotation = NormalizedQuaternion( transform.rotation );
			node.rotation = { rotation.x, rotation.y, rotation.z, rotation.w };
			node.scale = { transform.scale.x, transform.scale.y, transform.scale.z };
			model.nodes.push_back( node );
		}

		std::vector<int> rootNodeIndices;
		for( size_t boneIndex = 0; boneIndex < skeleton.bones.size(); boneIndex++ )
		{
			uint32_t parentIndex = skeleton.parents[boneIndex];
			int nodeIndex = boneNodeOffset + (int)boneIndex;
			if( parentIndex >= skeleton.bones.size() )
			{
				rootNodeIndices.push_back( nodeIndex );
			}
			else
			{
				model.nodes[boneNodeOffset + parentIndex].children.push_back( nodeIndex );
			}
		}

		int rootNodeIndex = -1;
		if( rootNodeIndices.size() == 1 )
		{
			rootNodeIndex = rootNodeIndices[0];
		}
		else if( rootNodeIndices.size() > 1 )
		{
			rootNodeIndex = (int)model.nodes.size();
			tinygltf::Node root;
			root.name = cmf::ToStdString( skeleton.name ) + "_root";
			root.children = rootNodeIndices;
			model.nodes.push_back( root );
		}

		result[skeletonIndex] = { boneNodeOffset, rootNodeIndex };
		if( rootNodeIndex >= 0 )
		{
			scene.nodes.push_back( rootNodeIndex );
		}
	}
	return result;
}

int BuildMeshSkin( const cmf::Mesh& mesh, const cmf::Skeleton& skeleton, const SkeletonNodes& skeletonNodes, tinygltf::Buffer& gltfBuffer, tinygltf::Model& model )
{
	const std::vector<int32_t> mapping = CreateMapping( skeleton, mesh.boneBindings );

	tinygltf::Skin skin;
	skin.name = cmf::ToStdString( skeleton.name );
	skin.skeleton = skeletonNodes.rootNodeIndex;

	std::vector<float> inverseBoneMatrices;
	inverseBoneMatrices.reserve( mesh.boneBindings.size() * 16 );

	for( size_t boneIndex = 0; boneIndex < mesh.boneBindings.size(); boneIndex++ )
	{
		int32_t bone = mapping[boneIndex];
		if( bone < 0 )
		{
			bone = 0;
		}

		skin.joints.push_back( skeletonNodes.boneNodeOffset + bone );

		auto matrix = (const float*)( &skeleton.invBindTransforms[bone] );
		inverseBoneMatrices.insert( inverseBoneMatrices.end(), matrix, matrix + 16 );
	}

	skin.inverseBindMatrices = AppendFloatAccessor( gltfBuffer, model, inverseBoneMatrices, TINYGLTF_TYPE_MAT4, false );

	model.skins.push_back( skin );
	return (int)model.skins.size() - 1;
}

void AddMorphWeightChannels( const cmf::Animation& animation, const std::vector<MorphMeshNode>& morphMeshNodes, tinygltf::Buffer& gltfBuffer, tinygltf::Model& model, tinygltf::Animation& gltfAnim )
{
	std::map<int, std::vector<const cmf::AnimationCurve*>> curvesGroupedByNode;

	for( const auto& channel : animation.channels )
	{
		if( channel.targetType != cmf::AnimationChannelTargetType::MorphTarget )
		{
			continue;
		}

		for( const auto& node : morphMeshNodes )
		{
			for( size_t target = 0; target < node.targets.size(); target++ )
			{
				cmf::String morphTargetName = node.targets[target].name;

				// TODO: intern, this "Shape" suffix will likely be gone later
				// By convention (due to the exporter), the morph target name ends with "Shape".
				std::string_view tmp = cmf::ToStdStringView( morphTargetName );
				if( tmp.size() > 5 && tmp.compare( tmp.size() - 5, 5, "Shape" ) == 0 )
				{
					morphTargetName.byteSize -= 5 * sizeof( char );
				}
				if( morphTargetName != channel.target )
				{
					continue;
				}

				auto it = curvesGroupedByNode.find( node.nodeIndex );
				if( it == curvesGroupedByNode.end() )
				{
					it = curvesGroupedByNode.emplace( node.nodeIndex, std::vector<const cmf::AnimationCurve*>( node.targets.size(), nullptr ) ).first;
				}
				it->second[target] = &animation.curves[channel.curveIndex];
			}
		}
	}

	for( auto& [nodeIndex, columns] : curvesGroupedByNode )
	{
		size_t numTargets = columns.size();

		std::set<float> timeSet;
		bool allStep = true;
		bool anyAnimated = false;
		for( size_t target = 0; target < numTargets; target++ )
		{
			if( !columns[target] )
			{
				continue;
			}
			anyAnimated = true;
			cmf::VertexElement knotElement{};
			knotElement.type = columns[target]->knotType;
			knotElement.elementCount = 1;
			uint32_t knotStride = cmf::GetVertexElementSize( knotElement );
			cmf::ConstBufferElementStream<float> knots{ knotElement, columns[target]->knots.data(), columns[target]->knotCount, knotStride };
			for( float knot : knots )
			{
				timeSet.insert( knot );
			}
			if( columns[target]->interpolation != cmf::Interpolation::Step )
			{
				allStep = false;
			}
		}
		if( !anyAnimated || timeSet.empty() )
		{
			continue;
		}

		std::vector<float> times( timeSet.begin(), timeSet.end() );
		size_t numFrames = times.size();

		std::vector<float> output( numFrames * numTargets, 0.0f );
		for( size_t frame = 0; frame < numFrames; frame++ )
		{
			for( size_t target = 0; target < numTargets; target++ )
			{
				if( columns[target] )
				{
					output[frame * numTargets + target] = cmf::SampleScalarCurve( *columns[target], times[frame] );
				}
			}
		}
		int inputAccessor = AppendFloatAccessor( gltfBuffer, model, times, TINYGLTF_TYPE_SCALAR, true );
		int outputAccessor = AppendFloatAccessor( gltfBuffer, model, output, TINYGLTF_TYPE_SCALAR, false );

		int samplerIndex = static_cast<int>( gltfAnim.samplers.size() );
		tinygltf::AnimationSampler sampler;
		sampler.input = inputAccessor;
		sampler.output = outputAccessor;
		sampler.interpolation = allStep ? "STEP" : "LINEAR";
		gltfAnim.samplers.push_back( sampler );

		tinygltf::AnimationChannel channel;
		channel.sampler = samplerIndex;
		channel.target_node = nodeIndex;
		channel.target_path = "weights";
		gltfAnim.channels.push_back( channel );
	}
}

void AddAnimations(
	CmfFile& cmfFile,
	tinygltf::Buffer& gltfBuffer,
	tinygltf::Model& model,
	const std::vector<MorphMeshNode>& morphMeshNodes )
{
	auto& data = cmfFile.GetData();

	for( const auto& animation : data.animations )
	{
		if( animation.channels.empty() )
		{
			continue;
		}

		tinygltf::Animation gltfAnim;
		gltfAnim.name = cmf::ToStdString( animation.name );

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
				// handled separately
				continue;
			case cmf::AnimationChannelTargetType::Other:
				printf( "Unsupported AnimationChannelTargetType for animation '%s' target '%s', continuing.\n", cmf::ToStdString( animation.name ).c_str(), cmf::ToStdString( channel.target ).c_str() );
				continue;
			}

			int gltfValueType = GetGLTFTypeFromComponentCount( curve.valueDimension );
			if( gltfValueType != TINYGLTF_TYPE_VEC3 && gltfValueType != TINYGLTF_TYPE_VEC4 )
			{
				continue;
			}

			// Find the target bone node by name
			const std::string targetName = cmf::ToStdString( channel.target );
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

			cmf::VertexElement valueElement = {};
			valueElement.type = curve.valueType;
			valueElement.elementCount = 1;
			const auto valueStride = cmf::GetVertexElementSize( valueElement );
			const cmf::ConstBufferElementStream<float> valueFloats{ valueElement, curve.values.data(), uint32_t( curve.values.size() / valueStride ), valueStride };

			// remove duplicate knots
			uint32_t dimension = curve.valueDimension;
			std::vector<float> times;
			std::vector<float> values;
			times.reserve( curve.knotCount );
			values.reserve( curve.knotCount * dimension );
			for( uint32_t i = 0; i < curve.knotCount; i++ )
			{
				if( !times.empty() && knotFloats[i] <= times.back() )
				{
					continue;
				}
				times.push_back( knotFloats[i] );
				for( uint32_t component = 0; component < dimension; component++ )
				{
					values.push_back( valueFloats[i * dimension + component] );
				}
			}

			int inputAccIdx = AppendFloatAccessor( gltfBuffer, model, times, TINYGLTF_TYPE_SCALAR, true );

			int outputAccIdx;
			if( channel.targetType == cmf::AnimationChannelTargetType::BoneRotation )
			{
				std::vector<float> normalizedValues = NormalizedQuaternionStream( values );
				outputAccIdx = AppendFloatAccessor( gltfBuffer, model, normalizedValues, gltfValueType, false );
			}
			else
			{
				outputAccIdx = AppendFloatAccessor( gltfBuffer, model, values, gltfValueType, false );
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

		AddMorphWeightChannels( animation, morphMeshNodes, gltfBuffer, model, gltfAnim );

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

	// Preprocess the cmf file and unpack the tangents into the t, b, n datasets
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

	// Convert morph targets from absolute to relative values
	for( auto& mesh : data.meshes )
	{
		for( auto& lod : mesh.lods )
		{
			for( auto morphTarget : lod.morphTargets )
			{
				for( auto elem : mesh.morphTargets.decl )
				{
					auto meshElem = cmf::FindElement( mesh.decl, elem.usage, elem.usageIndex );
					cmf::BufferElementStream<Vector4> morphStream( elem, morphTarget.vb, bufferManager );
					cmf::ConstBufferElementStream<Vector4> meshStream( *meshElem, lod.vb, bufferManager );
					for( uint32_t i = 0; i < morphStream.size(); i++ )
					{
						morphStream.set( i, morphStream[i] - meshStream[i] );
					}
				}
			}
		}
	}

	// Generate new decl for the mesh where all data is stored in float format
	for( auto& mesh : data.meshes )
	{
		auto generateNewDecl = [&allocator]( const cmf::Span<cmf::VertexElement>& oldDecl, uint8_t tangentElementCount, bool isMorphTarget ) -> cmf::Span<cmf::VertexElement> {
			cmf::Span<cmf::VertexElement> newDecl;
			for( const auto& elem : oldDecl )
			{
				if( isMorphTarget && ( elem.usage == cmf::Usage::BoneIndices || elem.usage == cmf::Usage::BoneWeights ) )
				{
					// glTF forbids JOINTS/WEIGHTS on morph targets
					continue;
				}
				if( elem.usage == cmf::Usage::Binormal )
				{
					// Skip binormal elements since we are converting them into tangents with binormal sign baked in according to the glTF spec
				}
				else if( elem.usage == cmf::Usage::Tangent )
				{
					// glTF requires TANGENT to be a float VEC4 with the binormal sign in the W component
					auto newElem = elem;
					newElem.type = cmf::ElementType::Float32;
					newElem.elementCount = tangentElementCount;
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
				else if( elem.usage == cmf::Usage::Position || elem.usage == cmf::Usage::Normal )
				{
					// glTF requires POSITION and NORMAL to be VEC3
					auto newElem = elem;
					newElem.type = cmf::ElementType::Float32;
					newElem.elementCount = 3;
					cmf::Modify( newDecl, allocator ).push_back( newElem );
				}
				else if( elem.usage == cmf::Usage::TexCoord )
				{
					// glTF requires TEXCOORD to be VEC2
					auto newElem = elem;
					newElem.type = cmf::ElementType::Float32;
					newElem.elementCount = 2;
					cmf::Modify( newDecl, allocator ).push_back( newElem );
				}
				else if( elem.type == cmf::ElementType::Float16 )
				{
					// We convert remaining attributes from float16 to float32
					auto newElem = elem;
					newElem.type = cmf::ElementType::Float32;
					cmf::Modify( newDecl, allocator ).push_back( newElem );
				}
				else
				{
					cmf::Modify( newDecl, allocator ).push_back( elem );
				}
			}

			if( !isMorphTarget && cmf::FindElement( oldDecl, cmf::Usage::BoneIndices ) && !cmf::FindElement( oldDecl, cmf::Usage::BoneWeights ) )
			{
				cmf::Modify( newDecl, allocator ).push_back( cmf::VertexElement{ cmf::Usage::BoneWeights, 0, cmf::ElementType::Float32, 4 } );
			}

			uint32_t offset = 0;
			for( auto& elem : newDecl )
			{
				offset = ( offset + 3u ) & ~3u;
				elem.offset = offset;
				offset += cmf::GetVertexElementSize( elem );
			}

			return newDecl;
		};
		cmf::Span<cmf::VertexElement> newDecl = generateNewDecl( mesh.decl, 4, false );
		cmf::Span<cmf::VertexElement> newMorphTargetsDecl = generateNewDecl( mesh.morphTargets.decl, 3, true );
		bool synthesizeWeights = cmf::FindElement( mesh.decl, cmf::Usage::BoneIndices ) && !cmf::FindElement( mesh.decl, cmf::Usage::BoneWeights );

		for( auto& lod : mesh.lods )
		{
			auto vb = cmf::ChangeBufferVertexDeclaration( lod.vb, mesh.decl, newDecl, allocator, bufferManager, 4 );
			for( const auto& elem : newDecl )
			{
				auto normalizedVector = []( Vector3 vector, Vector3 defaultValue ) -> Vector3 {
					float squaredLength = Dot( vector, vector );
					if( squaredLength < FLT_EPSILON )
					{
						return defaultValue;
					}
					if( std::fabs( squaredLength - 1.0f ) > FLT_EPSILON )
					{
						return Normalize( vector );
					}
					return vector;
				};

				if( elem.usage == cmf::Usage::Tangent )
				{
					const auto* binormalElem = cmf::FindElement( mesh.decl, cmf::Usage::Binormal, elem.usageIndex );
					const auto* normalElem = cmf::FindElement( mesh.decl, cmf::Usage::Normal, elem.usageIndex );
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
							tangents.set( i, Vector4{ normalizedVector( tangent, Vector3( 1.f, 0.f, 0.f ) ), w } );
						}
					}
					else
					{
						// No binormal/normal pair to derive the sign from, default W to +1 since glTF requires W = +-1
						const cmf::BufferElementStream<Vector4> tangents( elem, vb, bufferManager );
						for( uint32_t i = 0; i < tangents.size(); ++i )
						{
							tangents.set( i, Vector4{ normalizedVector( tangents[i].GetXYZ(), Vector3( 1.f, 0.f, 0.f ) ), 1.0f } );
						}
					}
				}
				if( elem.usage == cmf::Usage::Normal )
				{
					const cmf::BufferElementStream<Vector3> normals( elem, vb, bufferManager );
					for( uint32_t i = 0; i < normals.size(); i++ )
					{
						normals.set( i, normalizedVector( normals[i], Vector3( 0.f, 1.f, 0.f ) ) );
					}
				}
				if( elem.usage == cmf::Usage::Color )
				{
					// glTF requires COLOR components to be in the range [0,1]
					const cmf::BufferElementStream<Vector4> colors( elem, vb, bufferManager );
					for( uint32_t i = 0; i < colors.size(); i++ )
					{
						Vector4 c = colors[i];
						c.x = std::clamp( c.x, 0.0f, 1.0f );
						c.y = std::clamp( c.y, 0.0f, 1.0f );
						c.z = std::clamp( c.z, 0.0f, 1.0f );
						c.w = std::clamp( c.w, 0.0f, 1.0f );
						colors.set( i, c );
					}
				}
			}

			if( synthesizeWeights )
			{
				const auto* weightsElem = cmf::FindElement( newDecl, cmf::Usage::BoneWeights );
				const cmf::BufferElementStream<Vector4> weights( *weightsElem, vb, bufferManager );
				for( uint32_t i = 0; i < weights.size(); ++i )
				{
					weights.set( i, Vector4{ 1.0f, 0.0f, 0.0f, 0.0f } );
				}
			}

			const auto* boneIndicesElem = cmf::FindElement( newDecl, cmf::Usage::BoneIndices );
			const auto* boneWeightsElem = cmf::FindElement( newDecl, cmf::Usage::BoneWeights );
			if( boneIndicesElem && boneWeightsElem )
			{
				const cmf::BufferElementStream<std::array<uint32_t, 4>> boneIndices( *boneIndicesElem, vb, bufferManager );
				const cmf::ConstBufferElementStream<Vector4> boneWeights( *boneWeightsElem, vb, bufferManager );
				for( uint32_t i = 0; i < boneIndices.size(); i++ )
				{
					std::array<uint32_t, 4> indices = boneIndices[i];
					Vector4 weight = boneWeights[i];
					for( int j = 0; j < 4; j++ )
					{
						if( weight[j] == 0.0f && indices[j] != 0 )
						{
							indices[j] = 0;
						}
					}
					boneIndices.set( i, indices );
				}
			}

			lod.vb = vb;

			for( auto& morphTarget : lod.morphTargets )
			{
				morphTarget.vb = cmf::ChangeBufferVertexDeclaration( morphTarget.vb, mesh.morphTargets.decl, newMorphTargetsDecl, allocator, bufferManager, 4 );
			}
		}

		mesh.decl = newDecl;
		mesh.morphTargets.decl = newMorphTargetsDecl;
	}
}

bool IsMeshSkinned( const cmf::Mesh& mesh )
{
	return mesh.skeleton != 0xFF && !mesh.boneBindings.empty();
}

void AddMeshes( CmfFile& cmfFile, tinygltf::Buffer& gltfBuffer, tinygltf::Model& model, tinygltf::Scene& scene, std::vector<MorphMeshNode>& morphMeshNodes, const std::vector<SkeletonNodes>& skeletonNodes )
{
	auto& data = cmfFile.GetData();
	auto& bufferManager = cmfFile.GetBufferManager();
	auto& allocator = cmfFile.GetAllocator();

	for( size_t meshIndex = 0; meshIndex < data.meshes.size(); meshIndex++ )
	{
		const auto& mesh = data.meshes[meshIndex];

		const std::string meshName = cmf::ToStdString( mesh.name );

		int skinIndex = -1;
		if( IsMeshSkinned( mesh ) )
		{
			skinIndex = BuildMeshSkin( mesh, data.skeletons[mesh.skeleton], skeletonNodes[mesh.skeleton], gltfBuffer, model );
		}

		std::vector<int> lodNodeIndices;
		std::vector<uint32_t> lodThresholds;

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
				const uint32_t indexCount = cmf::GetStreamElementCount( lod.ib );

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
			auto processVertexData = [&gltfBuffer, &model, &vertexCount, &bufferManager](
										 std::map<std::string, int>& attributes,
										 const cmf::BufferView& vb,
										 const cmf::Span<cmf::VertexElement>& decl ) {
				const auto* vbBytes = static_cast<const uint8_t*>( bufferManager.GetData( vb ) );

				AlignBuffer( gltfBuffer, sizeof( float ) );
				const size_t byteOffset = gltfBuffer.data.size();
				const size_t byteLength = static_cast<size_t>( vb.size );
				gltfBuffer.data.resize( byteOffset + byteLength );

				uint8_t* dst = gltfBuffer.data.data() + byteOffset;

				// Since glTF supports interleaved data, just copy the whole buffer into the new glTF buffer
				memcpy( dst, vbBytes, vb.size );

				// A single interleaved buffer view shared by all the vertex attribute accessors
				const int vbBVIdx = static_cast<int>( model.bufferViews.size() );
				{
					tinygltf::BufferView bv;
					bv.buffer = 0;
					bv.byteOffset = byteOffset;
					bv.byteLength = byteLength;
					bv.target = TINYGLTF_TARGET_ARRAY_BUFFER;
					bv.byteStride = vb.stride;
					model.bufferViews.push_back( bv );
				}

				for( const auto& elem : decl )
				{
					const std::string name = GenerateAttributeName( attributes, elem.usage, elem.usageIndex );

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

						const cmf::ConstBufferElementStream<Vector4> stream( elem, vb, bufferManager );
						for( const auto& value : stream )
						{
							for( int j = 0; j < elem.elementCount; j++ )
							{
								minVals[j] = std::min( minVals[j], static_cast<double>( value[j] ) );
								maxVals[j] = std::max( maxVals[j], static_cast<double>( value[j] ) );
							}
						}

						acc.minValues = minVals;
						acc.maxValues = maxVals;
					}

					const int accIdx = static_cast<int>( model.accessors.size() );
					model.accessors.push_back( acc );
					attributes[name] = accIdx;
				}
			};
			processVertexData( prim.attributes, lod.vb, mesh.decl );

			prim.targets.resize( mesh.morphTargets.targets.size() );
			for( size_t i = 0; i < mesh.morphTargets.targets.size(); i++ )
			{
				processVertexData( prim.targets[i], lod.morphTargets[i].vb, mesh.morphTargets.decl );
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
				if( mesh.skeleton != 0xFF )
				{
					if( skinIndex >= 0 )
					{
						node.skin = skinIndex;
					}
				}

				if( !mesh.morphTargets.targets.empty() )
				{
					node.weights.resize( mesh.morphTargets.targets.size(), 0.f );
					morphMeshNodes.push_back( { nodeIdx, mesh.morphTargets.targets } );
				}

				model.nodes.push_back( node );
			}

			lodNodeIndices.push_back( nodeIdx );
			lodThresholds.push_back( lod.threshold );
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

			lodThresholds.push_back( 0 );
			tinygltf::Value::Array coverage;
			for( size_t i = 1; i < lodThresholds.size(); i++ )
			{
				double lowerBound = std::min( 1.0, double( lodThresholds[i] ) / MAX_SCREEN_SIZE );
				coverage.push_back( tinygltf::Value( lowerBound ) );
			}
			tinygltf::Value::Object extras;
			extras["MSFT_screencoverage"] = tinygltf::Value( coverage );
			model.nodes[lodNodeIndices[0]].extras = tinygltf::Value( extras );
		}

		scene.nodes.push_back( lodNodeIndices[0] );
	}
}

void GLTFConverter( CLI::App& app, GLTFOptions& options )
{
	app.add_option( "src", options.srcPath, "Path to the source CMF file" )->required()->check( CLI::ExistingFile );
	app.add_option( "dst", options.dstPath, "Path to the output glTF" )->required();
	app.add_flag( "--combinedfile", options.combinedFile, "Should we store the .bin data inside the gltf file as base64" );
	app.add_option( "--src2", options.srcPaths2, "Secondary source file(s) (i.e. containing animations); repeat the flag to add more than one" )->check( CLI::ExistingFile );
	app.final_callback( [&options]() {
		CmfFile cmfFile( options.srcPath );

		std::vector<std::unique_ptr<CmfFile>> secondaryFiles;
		secondaryFiles.reserve( options.srcPaths2.size() );
		for( const auto& path : options.srcPaths2 )
		{
			secondaryFiles.push_back( std::make_unique<CmfFile>( path ) );
		}

		tinygltf::Model model;
		tinygltf::TinyGLTF writer;

		tinygltf::Buffer gltfBuffer;
		tinygltf::Scene scene;

		std::vector<MorphMeshNode> morphMeshNodes;

		PreprocessCmfFile( cmfFile );
		for( auto& secondary : secondaryFiles )
		{
			PreprocessCmfFile( *secondary );
		}

		auto skeletonNodes1 = AddSkeletons( cmfFile, model, scene );
		AddMeshes( cmfFile, gltfBuffer, model, scene, morphMeshNodes, skeletonNodes1 );
		for( auto& secondary : secondaryFiles )
		{
			const auto& meshes = secondary->GetData().meshes;
			std::vector<SkeletonNodes> secondarySkeletonNodes;
			if( std::any_of( meshes.begin(), meshes.end(), IsMeshSkinned ) )
			{
				secondarySkeletonNodes = AddSkeletons( *secondary, model, scene );
			}
			AddMeshes( *secondary, gltfBuffer, model, scene, morphMeshNodes, secondarySkeletonNodes );
		}

		AddAnimations( cmfFile, gltfBuffer, model, morphMeshNodes );
		for( auto& secondary : secondaryFiles )
		{
			AddAnimations( *secondary, gltfBuffer, model, morphMeshNodes );
		}

		if( !gltfBuffer.data.empty() )
		{
			model.buffers.push_back( gltfBuffer );
		}
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

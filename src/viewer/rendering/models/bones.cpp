// Copyright © 2026 CCP ehf.

#include "bones.h"
#include "primitiveEffects.h"

namespace Bones
{

PrimitiveRenderable CreateJoint( std::shared_ptr<const Renderer> renderer, const cmf::Skeleton& skeleton, Vector3 color, Vector3 selectedColor )
{
	GraphicsEffectTypes::Config config{};
	config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;


	auto colorInfo = PrimitiveEffects::ColorInfo{
		Vector4( color, 1.0f ),
		Vector4( selectedColor, 1.0f )
	};

	auto vertexToBoneMapping = std::vector<uint32_t>( skeleton.bones.size(), 0 );
	for( uint32_t boneIndex = 0; boneIndex < skeleton.bones.size(); ++boneIndex )
	{
		vertexToBoneMapping[boneIndex] = boneIndex;
	}

	auto effect = PrimitiveEffects::CreateFlatColorEffect( renderer, colorInfo, config, vertexToBoneMapping );
	auto model = PrimitiveRenderable( renderer, std::move( effect ) );
	model.SetBufferData( reinterpret_cast<const uint8_t*>( JOINT_VERTICES.data() ), (uint8_t)( JOINT_VERTICES.size() * sizeof( Vector4 ) ), sizeof( Vector4 ) );
	model.SetIndexData( reinterpret_cast<const uint8_t*>( JOINT_INDICES.data() ), (uint8_t)( JOINT_INDICES.size() * sizeof( uint32_t ) ), sizeof( uint32_t ) );
	return model;
}

PrimitiveRenderable CreateBone( std::shared_ptr<const Renderer> renderer, const cmf::Skeleton& skeleton, Vector3 color, Vector3 selectedColor )
{
	GraphicsEffectTypes::Config config{};

	config.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	config.lineWidth = 1.5f;

	auto colorInfo = PrimitiveEffects::ColorInfo{
		Vector4( color, 0.0f ), Vector4( selectedColor, 0.0f )
	};
	auto vertexToBoneMapping = std::vector<uint32_t>( skeleton.bones.size() * 2, 0 );
	for( uint32_t boneIndex = 0; boneIndex < skeleton.bones.size(); ++boneIndex )
	{
		uint32_t parent = skeleton.parents[boneIndex];
		if( (int32_t)parent == -1 )
		{
			parent = boneIndex;
		}
		vertexToBoneMapping[boneIndex * 2] = boneIndex;
		vertexToBoneMapping[boneIndex * 2 + 1] = parent;
	}

	auto effect = PrimitiveEffects::CreateFlatColorEffect( renderer, colorInfo, config, vertexToBoneMapping );
	auto model = PrimitiveRenderable( renderer, std::move( effect ) );
	model.SetBufferData( reinterpret_cast<const uint8_t*>( BONE_VERTICES.data() ), (uint8_t)( BONE_VERTICES.size() * sizeof( Vector4 ) ), sizeof( Vector4 ) );
	model.SetIndexData( reinterpret_cast<const uint8_t*>( BONE_INDICES.data() ), (uint8_t)( BONE_INDICES.size() * sizeof( uint32_t ) ), sizeof( uint32_t ) );
	return model;
}

}

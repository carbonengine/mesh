// Copyright © 2026 CCP ehf.

#include "primitiveEffects.h"

GraphicsEffect PrimitiveEffects::CreateFlatColorEffect( std::shared_ptr<const Renderer> renderer, ColorInfo colorInfo, GraphicsEffect::Config config, std::vector<uint32_t> vertexToBoneMapping )
{
	config.availableVertexElements = {
		{ cmf::Usage::Position,
		  0,
		  cmf::ElementType::Float32,
		  4,
		  0 }
	};
	config.stride = sizeof( Vector4 );
	auto effect = GraphicsEffect( renderer );
	effect.RegisterUniformData<PrimitiveEffects::VertexUBO>( VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, 0 );
	effect.RegisterStorageBuffer<Matrix>( VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, 1, 0xFF ); // bone transforms
	effect.RegisterStorageBuffer<uint32_t>( VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, 2, vertexToBoneMapping.data(), vertexToBoneMapping.size() ); // vertex to bone mapping
	effect.RegisterStorageBuffer<uint32_t>( VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, 3, 0xFF ); // selected bones
	effect.RegisterUniformData<ColorInfo>( VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, 4, colorInfo );
	effect.SetConfig( config );
	effect.SetShaderName( "flatcolor" );
	return effect;
}

GraphicsEffect PrimitiveEffects::CreateAxisEffect( std::shared_ptr<const Renderer> renderer )
{
	auto config = GraphicsEffect::Config();
	config.availableVertexElements = {
		{ cmf::Usage::Position,
		  0,
		  cmf::ElementType::Float32,
		  3,
		  0 },
		{ cmf::Usage::Color,
		  0,
		  cmf::ElementType::Float32,
		  3,
		  sizeof( Vector3 ) },
	};
	config.lineWidth = 2.0f;
	config.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	config.stride = sizeof( Vector3 ) * 2;
	auto effect = GraphicsEffect( renderer );
	effect.RegisterUniformData<PrimitiveEffects::VertexUBO>( VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, 0 );
	effect.RegisterStorageBuffer<Matrix>( VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, 1, 0xFF );
	effect.SetConfig( config );
	effect.SetShaderName( "orientationgizmo" );
	return effect;
}
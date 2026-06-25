// Copyright © 2026 CCP ehf.

#include "primitiveEffects.h"
#include <cmf/declutils.h>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <Matrix.h>
#include <Sphere.h>
#include <Vector3.h>
#include <Vector4.h>
#include <vulkan/vulkan_core.h>
#include <cmf/v1.h>
#include <rendering/renderer.h>
#include <rendering/vulkan/graphicseffect.h>
#include <rendering/vulkan/graphicseffecttypes.h>

namespace
{
constexpr float AXIS_LENGTH_SCALE = 0.005f;
constexpr float AXIS_LENGTH_MIN_SIZE = 0.001f;

GraphicsEffect CreateAxisEffect( std::shared_ptr<const Renderer> renderer, const cmf::Usage usage, uint32_t usageIndex, const cmf::Mesh& mesh )
{
	auto effectConfig = GraphicsEffectTypes::Config();
	PrimitiveEffects::AxisConfig axisConfig{};
	auto inputDeclaration = GraphicsEffectTypes::ShaderInputDeclaration();
	inputDeclaration.vertexInputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

	for( const auto& element : mesh.decl )
	{
		inputDeclaration.stride += cmf::GetVertexElementSize( element );
		if( element.usage == usage || element.usage == cmf::Usage::PackedTangent || element.usage == cmf::Usage::PackedTangentLegacy )
		{
			if( element.usageIndex == usageIndex )
			{
				inputDeclaration.vertexDeclarations.push_back( element );
			}
		}
		else
		{
			inputDeclaration.vertexDeclarations.push_back( element );
		}
	}
	effectConfig.inputDeclaration = inputDeclaration;
	effectConfig.lineWidth = 2.0f;
	effectConfig.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

	assert( usage == cmf::Usage::Normal || usage == cmf::Usage::Tangent || usage == cmf::Usage::Binormal );
	switch( usage )
	{
	case cmf::Usage::Normal:
		axisConfig.color = Vector3( 0, 1, 0 );
		axisConfig.axisIndex = 0;
		break;
	case cmf::Usage::Tangent:
		axisConfig.color = Vector3( 1, 0, 0 );
		axisConfig.axisIndex = 1;
		break;
	case cmf::Usage::Binormal:
		axisConfig.color = Vector3( 0, 0, 1 );
		axisConfig.axisIndex = 2;
		break;
	default:
		throw std::runtime_error( "Unsupported usage for CreateUnpackedAxisEffect" );
	}

	axisConfig.scale = std::max( AXIS_LENGTH_MIN_SIZE, CcpMath::Sphere( mesh.bounds ).radius * AXIS_LENGTH_SCALE );

	auto effect = GraphicsEffect( renderer );
	effect.RegisterUniformData<GraphicsEffect::VertexUboData>( VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, 0 );
	// this is what axis to draw, the shader will decode the packed tangent and use the sign to determine if it's normal, tangent or bitangent
	effect.RegisterUniformData<PrimitiveEffects::AxisConfig>( VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, 1, axisConfig );
	effect.SetConfig( effectConfig );
	return effect;
}
}

GraphicsEffect PrimitiveEffects::CreateFlatColorEffect( std::shared_ptr<const Renderer> renderer, ColorInfo colorInfo, GraphicsEffectTypes::Config config, std::vector<uint32_t> vertexToBoneMapping )
{
	config.inputDeclaration.stride = sizeof( Vector4 );
	config.inputDeclaration.vertexDeclarations = {
		cmf::VertexElement{ cmf::Usage::Position, 0, cmf::ElementType::Float32, 4, 0 }
	};

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

GraphicsEffect PrimitiveEffects::CreateOrientationEffect( std::shared_ptr<const Renderer> renderer )
{
	auto config = GraphicsEffectTypes::Config();
	config.inputDeclaration.vertexDeclarations = {
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
	config.inputDeclaration.stride = sizeof( Vector3 ) * 2;
	config.lineWidth = 2.0f;
	config.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

	auto effect = GraphicsEffect( renderer );
	effect.RegisterUniformData<PrimitiveEffects::VertexUBO>( VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, 0 );
	effect.RegisterStorageBuffer<Matrix>( VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, 1, 0xFF );
	effect.SetConfig( config );
	effect.SetShaderName( "orientationgizmo" );
	return effect;
}

GraphicsEffect PrimitiveEffects::CreateUnpackedAxisEffect( std::shared_ptr<const Renderer> renderer, const cmf::Usage usage, uint32_t usageIndex, const cmf::Mesh& mesh )
{
	std::string shaderName;
	switch( usage )
	{
	case cmf::Usage::Normal:
		shaderName = "normalaxis";
		break;
	case cmf::Usage::Tangent:
		shaderName = "tangentaxis";
		break;
	case cmf::Usage::Binormal:
		shaderName = "binormalaxis";
		break;
	default:
		throw std::runtime_error( "Unsupported usage for CreateUnpackedAxisEffect" );
	}
	auto effect = CreateAxisEffect( renderer, usage, usageIndex, mesh );
	effect.SetShaderName( shaderName );
	return effect;
}

GraphicsEffect PrimitiveEffects::CreatePackedAxisEffect( std::shared_ptr<const Renderer> renderer, const cmf::Usage usage, uint32_t usageIndex, const cmf::Mesh& mesh )
{
	auto effect = CreateAxisEffect( renderer, usage, usageIndex, mesh );
	effect.SetShaderName( "packedtangentaxis" );
	return effect;
}

GraphicsEffect PrimitiveEffects::CreatePackedLegacyAxisEffect( std::shared_ptr<const Renderer> renderer, const cmf::Usage usage, uint32_t usageIndex, const cmf::Mesh& mesh )
{
	auto effect = CreateAxisEffect( renderer, usage, usageIndex, mesh );
	effect.SetShaderName( "packedtangentlegacyaxis" );
	return effect;
}
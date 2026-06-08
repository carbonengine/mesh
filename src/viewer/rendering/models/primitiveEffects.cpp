#include "primitiveEffects.h"
#include <cmf/declutils.h>

const float AXIS_LENGTH_SCALE = 0.005f;
const float AXIS_LENGTH_MIN_SIZE = 0.001f;

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

GraphicsEffect CreateAxisEffect( std::shared_ptr<const Renderer> renderer, const cmf::Usage usage, const cmf::Mesh& mesh )
{
	auto effectConfig = GraphicsEffect::Config();
	PrimitiveEffects::AxisConfig axisConfig;

	uint32_t stride = 0;
	// find the position and normal elements in the vertex declaration,
	for( const auto& element : mesh.decl )
	{
		stride += cmf::GetVertexElementSize( element );
		effectConfig.availableVertexElements.push_back( element );
	}
	effectConfig.lineWidth = 2.0f;
	effectConfig.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	effectConfig.stride = stride;
	effectConfig.vertexInputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

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

GraphicsEffect PrimitiveEffects::CreateUnpackedAxisEffect( std::shared_ptr<const Renderer> renderer, const cmf::Usage usage, const cmf::Mesh& mesh )
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
	auto effect = CreateAxisEffect( renderer, usage, mesh );
	effect.SetShaderName( shaderName );
	return effect;
}

GraphicsEffect PrimitiveEffects::CreatePackedAxisEffect( std::shared_ptr<const Renderer> renderer, const cmf::Usage usage, const cmf::Mesh& mesh )
{
	auto effect = CreateAxisEffect( renderer, usage, mesh );
	effect.SetShaderName( "packedtangentaxis" );
	return effect;
}

GraphicsEffect PrimitiveEffects::CreatePackedLegacyAxisEffect( std::shared_ptr<const Renderer> renderer, const cmf::Usage usage, const cmf::Mesh& mesh )
{
	auto effect = CreateAxisEffect( renderer, usage, mesh );
	effect.SetShaderName( "packedtangentlegacyaxis" );
	return effect;
}
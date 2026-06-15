#pragma once

namespace GraphicsEffectTypes
{

struct ShaderInputDeclaration
{
	VkVertexInputRate vertexInputRate{ VK_VERTEX_INPUT_RATE_VERTEX };
	std::vector<cmf::VertexElement> vertexDeclarations;
	size_t stride{ 0 };
	// used for displaying additional info in the UI, f.ex Color 1, UV 2, etc.
	std::string shaderNameAddition{ "" };

	bool operator==( const ShaderInputDeclaration& other ) const
	{
		if( vertexInputRate != other.vertexInputRate || stride != other.stride || shaderNameAddition != other.shaderNameAddition )
		{
			return false;
		}
		if( vertexDeclarations.size() != other.vertexDeclarations.size() )
		{
			return false;
		}
		for( size_t i = 0; i < vertexDeclarations.size(); i++ )
		{
			if( vertexDeclarations[i].usage != other.vertexDeclarations[i].usage ||
				vertexDeclarations[i].usageIndex != other.vertexDeclarations[i].usageIndex ||
				vertexDeclarations[i].type != other.vertexDeclarations[i].type ||
				vertexDeclarations[i].elementCount != other.vertexDeclarations[i].elementCount ||
				vertexDeclarations[i].offset != other.vertexDeclarations[i].offset )
			{
				return false;
			}
		}
		return true;
	}
};

struct Config
{
	VkPrimitiveTopology topology{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };
	VkPolygonMode polygonMode{ VK_POLYGON_MODE_FILL };
	float lineWidth{ 1.0f };
	VkCompareOp depthCompareOp{ VK_COMPARE_OP_LESS };
	VkCullModeFlags cullMode{ VK_CULL_MODE_BACK_BIT };
	bool blend{ false };
	ShaderInputDeclaration inputDeclaration;
};

}
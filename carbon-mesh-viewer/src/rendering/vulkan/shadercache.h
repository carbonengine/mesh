#pragma once

#include <map>
#include <optional>
#include <tuple>
#include <vector>

#include "device.h"
#include "../renderer.h"

class Renderer;

class Shader
{
public:
	Shader();
	Shader( std::vector<uint32_t> code );

	VkResult Initialize( const Renderer* renderer, VkShaderStageFlagBits shaderFlag );
	VkResult Release( const Renderer* renderer );

private:
	VkShaderModule m_module;
	VkPipelineShaderStageCreateInfo m_stageInfo;
	std::vector<uint32_t> m_code;

	friend class ShaderCache;
};

// Shader cache which holds on to shader modules and creates pipelines
class ShaderCache
{
public:
	struct PipelineConfig
	{
		VkPrimitiveTopology topology{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };
		VkPolygonMode polygonMode{ VK_POLYGON_MODE_FILL };
		float lineWidth{ 1.0f };
		VkCompareOp depthCompareOp{ VK_COMPARE_OP_LESS };
		VkCullModeFlags cullMode{ VK_CULL_MODE_BACK_BIT };
 		bool blend{ false };
	};

	ShaderCache( std::shared_ptr<Renderer> renderer );
	~ShaderCache();

	VkResult Initialize();
	VkResult CreatePipeline( std::string shaderName, PipelineConfig config, uint32_t stride, std::vector<VkVertexInputAttributeDescription> vertexDescriptions, VkPipeline* outPipeline ) const;

	VkResult CreatePipelineLayout();

	VkPipelineLayout GetPipelineLayout() const;
	VkDescriptorSetLayout GetDescriptorSetLayout() const;

	static std::vector<std::string> GetAvailableShaderNames();

private:
	static std::map<std::string, std::tuple<std::optional<Shader>, std::optional<Shader>>> s_cache;

	std::shared_ptr<Renderer> m_renderer;
	VkPipelineLayout m_pipelineLayout;
	VkDescriptorSetLayout m_descriptorSetLayout;
};

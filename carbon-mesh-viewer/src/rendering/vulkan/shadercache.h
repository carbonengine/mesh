#pragma once

#include <map>
#include <optional>
#include <tuple>
#include <vector>

#include "device.h"
#include "../renderer.h"


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
	ShaderCache();

	VkResult Release( const Renderer* renderer );
	VkResult Initialize( const Renderer* renderer );
	VkResult CreatePipeline( const Renderer* renderer, std::string shaderName, VkPolygonMode mode, uint32_t stride, std::vector<VkVertexInputAttributeDescription> vertexDescriptions, VkPipeline* outPipeline );
	VkResult CreatePipelineLayout( const Renderer* renderer );

	VkPipelineLayout GetPipelineLayout() const;
	VkDescriptorSetLayout GetDescriptorSetLayout() const;

	static std::vector<std::string> GetAvailableShaderNames();

private:
	static std::map<std::string, std::tuple<std::optional<Shader>, std::optional<Shader>>> s_cache;

	VkPipelineLayout m_pipelineLayout;
	VkDescriptorSetLayout m_descriptorSetLayout;
};

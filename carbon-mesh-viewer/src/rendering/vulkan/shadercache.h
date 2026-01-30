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
	ShaderCache( std::shared_ptr<Renderer> renderer );
	~ShaderCache();

	VkResult Initialize();
	VkResult CreatePipeline( std::string shaderName, VkPrimitiveTopology topology, VkPolygonMode mode, float lineWidth, uint32_t stride, std::vector<VkVertexInputAttributeDescription> vertexDescriptions, VkPipeline* outPipeline ) const;

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

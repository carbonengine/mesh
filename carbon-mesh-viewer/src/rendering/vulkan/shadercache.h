#pragma once
#include <optional>
#include <vulkan/vulkan.h>
#include <map>
#include <tuple>
#include <vector>
#include "device.h"
#include "../renderer.h"

class Shader
{
public:
	Shader();
	Shader( std::vector<uint32_t> code );
	~Shader() = default;

	VkResult Initialize( const Renderer* renderer, VkShaderStageFlagBits shaderFlag );
	VkResult Release( const Renderer* renderer );

private:
	VkShaderModule m_module;
	VkPipelineShaderStageCreateInfo m_stageInfo;
	std::vector<uint32_t> m_code;

	friend class ShaderCache;
};

class ShaderCache
{
public:
	ShaderCache();
	~ShaderCache() = default;

	VkResult Release( const Renderer* renderer );
	VkResult Initialize( const Renderer* renderer );
	VkResult CreatePipeline( const Renderer* renderer, std::string shaderName, VkPolygonMode mode, uint32_t stride, std::vector<VkVertexInputAttributeDescription> vertexDescriptions, VkPipeline* outPipeline );
	VkResult CreatePipelineLayout( const Renderer* renderer );

	VkPipelineLayout GetPipelineLayout() const;
	VkDescriptorSetLayout GetDescriptorSetLayout() const;

private:
	static std::map<std::string, std::tuple<std::optional<Shader>, std::optional<Shader>>> s_cache;

	VkPipelineLayout m_pipelineLayout;
	VkDescriptorSetLayout m_descriptorSetLayout;
};

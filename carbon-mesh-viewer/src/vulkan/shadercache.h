#pragma once
#include <optional>
#include <vulkan/vulkan.h>
#include <map>
#include <tuple>
#include <vector>
#include "device.h"

class Shader
{
public:
	Shader();
	Shader( std::vector<uint32_t> code );
	~Shader() = default;

	VkResult Initialize( VkDevice device, VkShaderStageFlagBits shaderFlag );
	VkResult Release( VkDevice device, VkAllocationCallbacks* allocator );

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

	VkResult Release( VkDevice device, VkAllocationCallbacks* allocator );
	VkResult Initialize( VkDevice device );
	VkResult CreatePipeline( VkDevice device, std::string shaderName, VkPolygonMode mode, VkRenderPass renderPass, uint32_t stride, std::vector<VkVertexInputAttributeDescription> vertexDescriptions, VkPipeline* outPipeline );
	VkResult CreatePipelineLayout( VkDevice device, VkDescriptorSetLayout descriptorSetLayout );

    VkPipelineLayout GetPipelineLayout() const;

private:
	static std::map<std::string, std::tuple<std::optional<Shader>, std::optional<Shader>>> s_cache;

    VkPipelineLayout m_pipelineLayout;
};

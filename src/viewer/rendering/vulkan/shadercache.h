#pragma once

#include <map>
#include <optional>
#include <tuple>
#include <vector>

#include "device.h"
#include "../renderer.h"
#include "commandbuffer.h"
#include "graphicseffect.h"

class Renderer;

class Shader
{
public:
	Shader();
	Shader( std::vector<uint32_t> code );

	VkResult Initialize( const Renderer* renderer, VkShaderStageFlagBits shaderFlag );

private:
	friend class ShaderCache;
	VkShaderModule m_module = VK_NULL_HANDLE;
	VkPipelineShaderStageCreateInfo m_stageInfo{};
	std::vector<uint32_t> m_code{};
};

struct ShaderInputLayout
{
	uint8_t location;
	cmf::Usage usage;
	uint8_t usageIndex;
};

struct ShaderContainer
{
	std::optional<Shader> vertex = std::nullopt;
	std::optional<Shader> fragment = std::nullopt;
	std::optional<Shader> compute = std::nullopt;
	bool isModelShader = false;
	std::vector<ShaderInputLayout> inputLayout{};
};

// Shader cache which holds on to shader modules and creates pipelines
class ShaderCache
{
public:
	static VkResult InitializeShaders( const Renderer* renderer );
	static void ReleaseShaders( const Renderer* renderer );

	static VkResult CreateGraphicsPipeline( const Renderer* renderer, std::string shaderName, GraphicsEffect::Config config, VkPipelineLayout pipelineLayout, VkPipeline* outPipeline );
	static VkResult CreateComputePipeline( const Renderer* renderer, std::string shaderName, VkPipelineLayout pipelineLayout, VkPipeline* outPipeline );
	static std::vector<std::string> GetAvailableShaderNames( const std::vector<cmf::VertexElement>& availableVertexElements );

	static std::vector<cmf::Usage> GetShaderUsage( std::string shaderName );

private:
	static void GenerateVertexDescriptions( std::string shaderName, const std::vector<cmf::VertexElement>& availableVertexElements, std::vector<VkVertexInputAttributeDescription>* outAttributeDescriptions );

	static std::map<std::string, ShaderContainer> s_cache;
	static bool s_initialized;
};

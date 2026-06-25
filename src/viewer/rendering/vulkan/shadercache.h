// Copyright © 2025 CCP ehf.

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
	// Used if we can support mapping of multiple vertex elements that have the same usage.
	// e.g multiple UV sets, multiple color sets, etc.
	bool multiUsageIndex;
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

	static VkResult CreateGraphicsPipeline( const Renderer* renderer, std::string shaderName, GraphicsEffectTypes::Config config, VkPipelineLayout pipelineLayout, VkPipeline* outPipeline );
	static VkResult CreateComputePipeline( const Renderer* renderer, std::string shaderName, VkPipelineLayout pipelineLayout, VkPipeline* outPipeline );
	static std::vector<std::pair<std::string, GraphicsEffectTypes::ShaderInputDeclaration>> GetAvailableShaders( const std::vector<cmf::VertexElement>& availableVertexElements );

private:
	static void GenerateVertexDescriptions( std::string shaderName, const std::vector<cmf::VertexElement>& availableVertexElements, std::vector<VkVertexInputAttributeDescription>* outAttributeDescriptions );

	static std::map<std::string, ShaderContainer> s_cache;
	static bool s_initialized;
};

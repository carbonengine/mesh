#pragma once

#include <map>
#include <optional>
#include <tuple>
#include <vector>

#include "device.h"
#include "../renderer.h"
#include "commandbuffer.h"
#include "effect.h"

class Renderer;

class Shader
{
public:
	Shader();
	Shader( std::vector<uint32_t> code );

	VkResult Initialize( const Renderer* renderer, VkShaderStageFlagBits shaderFlag );

private:
	friend class ShaderCache;
	VkShaderModule m_module;
	VkPipelineShaderStageCreateInfo m_stageInfo;
	std::vector<uint32_t> m_code;
};

// Shader cache which holds on to shader modules and creates pipelines
class ShaderCache
{
public:
	static VkResult InitializeShaders( const Renderer* renderer );
	static void ReleaseShaders( const Renderer* renderer );

	static VkResult CreatePipeline( const Renderer* renderer, std::string shaderName, Effect::Config config, VkPipelineLayout pipelineLayout, VkPipeline* outPipeline );
	static std::vector<std::string> GetAvailableShaderNames();

private:
	static std::map<std::string, std::tuple<std::optional<Shader>, std::optional<Shader>>> s_cache;
	static bool s_initialized;
};

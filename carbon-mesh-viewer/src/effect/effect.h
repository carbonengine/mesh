#pragma once
#include "device.h"
#include <vulkan/vulkan.h>

class Effect
{
public:
	Effect();
	Effect( VkShaderModule vertex, VkShaderModule fragment );

	~Effect();
	void Release( Device* device );

	std::vector<VkPipelineShaderStageCreateInfo> GetShaderStages() const;

private:
	VkShaderModule m_vertexShaderModule;
	VkShaderModule m_fragmentShaderModule;

	VkPipelineShaderStageCreateInfo m_vertexShaderStageInfo;
	VkPipelineShaderStageCreateInfo m_fragmentShaderStageInfo;
};

namespace EffectLoader
{
Effect* LoadEffectFromFile( Device* device, const std::string& vertexShader, const std::string& fragmentShader );
}
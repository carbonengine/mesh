#include "effect.h"
#include <fstream>

namespace EffectLoader
{
// pass in the name of the authored file
Effect* LoadEffectFromFile( Device* device, const std::string& vertexShader, const std::string& fragmentShader )
{

	CCP_LOGNOTICE( "Loading shader from: %s and %s", vertexShader.c_str(), fragmentShader.c_str() );
	auto logicalDevice = device->GetLogicalDevice();
	auto loadShader = [logicalDevice]( const std::string& shaderFile ) -> VkShaderModule {
		std::string fullpath = "C:\\Users\\olafurth\\github.com\\carbon-mesh\\.cmake-build-Visual Studio 17 2022-carbon_windows_vcpkg_vs\\carbon-mesh-viewer\\Debug\\shaders\\" + shaderFile;
		std::ifstream is( fullpath.c_str(), std::ios::binary | std::ios::in | std::ios::ate );

		if( is.is_open() )
		{
			size_t size;
			char* code{ nullptr };
			size = is.tellg();
			is.seekg( 0, std::ios::beg );
			// Copy file contents into a buffer
			code = new char[size];
			is.read( code, size );
			is.close();

			// Create a new shader module that will be used for pipeline creation
			VkShaderModuleCreateInfo shaderModuleCI{};
			shaderModuleCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			shaderModuleCI.codeSize = size;
			shaderModuleCI.pCode = (uint32_t*)code;

			VkShaderModule shaderModule;
			if( CR( vkCreateShaderModule( logicalDevice, &shaderModuleCI, nullptr, &shaderModule ) ) != VK_SUCCESS )
			{
				delete[] code;
				CCP_LOGERR( "Failed to create shader module for file: %s", fullpath.c_str() );
				return VK_NULL_HANDLE;
			}
			delete[] code;

			return shaderModule;
		}
		CCP_LOGERR( "Failed to load shader file: %s", fullpath.c_str() );

		return VK_NULL_HANDLE;
	};

	VkShaderModule vertexModule = loadShader( vertexShader );
	VkShaderModule fragmentModule = loadShader( fragmentShader );

	if( vertexModule == VK_NULL_HANDLE || fragmentModule == VK_NULL_HANDLE )
	{
		return nullptr;
	}
	return new Effect( vertexModule, fragmentModule );
}
};

Effect::Effect() :
	m_vertexShaderModule( VK_NULL_HANDLE ),
	m_fragmentShaderModule( VK_NULL_HANDLE ),
	m_vertexShaderStageInfo( {} ),
	m_fragmentShaderStageInfo( {} )
{
}

Effect::Effect( VkShaderModule vertex, VkShaderModule fragment ) :
	m_vertexShaderModule( vertex ),
	m_fragmentShaderModule( fragment ),
	m_vertexShaderStageInfo( {} ),
	m_fragmentShaderStageInfo( {} )
{
	m_vertexShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	// Set pipeline stage for this shader
	m_vertexShaderStageInfo.stage = VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT;
	// Load binary SPIR-V shader
	m_vertexShaderStageInfo.module = m_vertexShaderModule;
	// Main entry point for the shader
	m_vertexShaderStageInfo.pName = "main";

	m_fragmentShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	// Set pipeline stage for this shader
	m_fragmentShaderStageInfo.stage = VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT;
	// Load binary SPIR-V shader
	m_fragmentShaderStageInfo.module = m_fragmentShaderModule;
	// Main entry point for the shader
	m_fragmentShaderStageInfo.pName = "main";
}

Effect::~Effect()
{
}

std::vector<VkPipelineShaderStageCreateInfo> Effect::GetShaderStages() const
{
	return { m_vertexShaderStageInfo, m_fragmentShaderStageInfo };
}

void Effect::Release( Device* device )
{
	if( m_fragmentShaderModule != VK_NULL_HANDLE )
	{
		vkDestroyShaderModule( device->GetLogicalDevice(), m_fragmentShaderModule, nullptr );
	}
	if( m_vertexShaderModule != VK_NULL_HANDLE )
	{
		vkDestroyShaderModule( device->GetLogicalDevice(), m_vertexShaderModule, nullptr );
	}
}

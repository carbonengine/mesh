// Copyright © 2025 CCP ehf.

#include "shadercache.h"

#include <cmf/declutils.h>

#include "vulkanerrors.h"
#include "vulkanenums.h"

std::map<std::string, ShaderContainer> ShaderCache::s_cache = {
#include "generatedShaderCache.h"
};
bool ShaderCache::s_initialized = false;

Shader::Shader()
{
	Log::Error( "Default Shader constructor called. This is likely an error." );
}

Shader::Shader( std::vector<uint32_t> code )
{
	m_code = code;
}

VkResult Shader::Initialize( const Renderer* renderer, VkShaderStageFlagBits shaderFlag )
{
	VkShaderModuleCreateInfo shaderModuleCI = {};
	shaderModuleCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCI.codeSize = m_code.size() * sizeof( uint32_t );
	shaderModuleCI.pCode = m_code.data();

	VkResult result = vkCreateShaderModule( renderer->GetDevice()->GetLogicalDevice(), &shaderModuleCI, renderer->GetAllocator(), &m_module );
	if( result != VK_SUCCESS )
	{
		return result;
	}
	m_stageInfo = {};
	m_stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	m_stageInfo.stage = shaderFlag;
	m_stageInfo.module = m_module;
	m_stageInfo.pName = "main";

	return VK_SUCCESS;
}

VkResult ShaderCache::InitializeShaders( const Renderer* renderer )
{
	assert( !s_initialized );
	for( auto& [key, shaderContainer] : s_cache )
	{
		if( shaderContainer.vertex.has_value() )
		{
			CR_RETURN( shaderContainer.vertex.value().Initialize( renderer, VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT ) );
		}
		if( shaderContainer.fragment.has_value() )
		{
			CR_RETURN( shaderContainer.fragment.value().Initialize( renderer, VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT ) );
		}
		if( shaderContainer.compute.has_value() )
		{
			CR_RETURN( shaderContainer.compute.value().Initialize( renderer, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT ) );
		}
	}
	s_initialized = true;
	return VK_SUCCESS;
}

void ShaderCache::ReleaseShaders( const Renderer* renderer )
{
	assert( s_initialized );
	auto allocator = renderer->GetAllocator();
	auto logicalDevice = renderer->GetDevice()->GetLogicalDevice();

	for( auto& [key, shaderContainer] : s_cache )
	{
		if( shaderContainer.vertex.has_value() && shaderContainer.vertex->m_module != VK_NULL_HANDLE )
		{
			vkDestroyShaderModule( logicalDevice, shaderContainer.vertex->m_module, allocator );
		}
		if( shaderContainer.fragment.has_value() && shaderContainer.fragment->m_module != VK_NULL_HANDLE )
		{
			vkDestroyShaderModule( logicalDevice, shaderContainer.fragment->m_module, allocator );
		}
		if( shaderContainer.compute.has_value() && shaderContainer.compute->m_module != VK_NULL_HANDLE )
		{
			vkDestroyShaderModule( logicalDevice, shaderContainer.compute->m_module, allocator );
		}
	}
	s_initialized = false;
}

VkResult ShaderCache::CreateGraphicsPipeline( const Renderer* renderer, std::string shaderName, GraphicsEffect::Config config, VkPipelineLayout pipelineLayout, VkPipeline* outPipeline )
{
	assert( s_initialized );

	auto it = s_cache.find( shaderName );
	if( it == s_cache.end() )
	{
		Log::Error( "Shader \"%s\" not found in cache", shaderName.c_str() );
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	auto& shaderContainer = it->second;
	if( !shaderContainer.vertex.has_value() || !shaderContainer.fragment.has_value() )
	{
		Log::Error( "Shader \"%s\" is missing a vertex or fragment shader", shaderName.c_str() );
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{};
	inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyState.primitiveRestartEnable = VK_FALSE;
	inputAssemblyState.flags = 0;
	inputAssemblyState.pNext = nullptr;
	inputAssemblyState.topology = config.topology;

	VkPipelineRasterizationStateCreateInfo rasterizationState{};
	rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationState.polygonMode = config.polygonMode;
	rasterizationState.cullMode = config.cullMode;
	rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationState.lineWidth = config.lineWidth;

	VkPipelineColorBlendAttachmentState blendAttachmentState{};
	blendAttachmentState.colorWriteMask = 0xf;
	blendAttachmentState.blendEnable = config.blend;
	if( config.blend )
	{
		blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
		blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
	}

	VkPipelineColorBlendStateCreateInfo colorBlendState{};
	colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendState.attachmentCount = 1;
	colorBlendState.pAttachments = &blendAttachmentState;

	VkPipelineDepthStencilStateCreateInfo depthStencilState{};
	depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilState.depthTestEnable = VK_TRUE;
	depthStencilState.depthWriteEnable = VK_TRUE;
	depthStencilState.depthCompareOp = config.depthCompareOp;
	depthStencilState.back.compareOp = VK_COMPARE_OP_ALWAYS;

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;
	viewportState.flags = 0;

	VkPipelineMultisampleStateCreateInfo multisampleState{};
	multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleState.flags = 0;

	std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_LINE_WIDTH };

	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.pDynamicStates = &dynamicStateEnables[0];
	dynamicState.dynamicStateCount = static_cast<uint32_t>( dynamicStateEnables.size() );
	dynamicState.flags = 0;

	std::vector<VkVertexInputAttributeDescription> vertexDescriptions;
	ShaderCache::GenerateVertexDescriptions( shaderName, config.availableVertexElements, &vertexDescriptions );

	VkVertexInputBindingDescription bindingDecl{};
	bindingDecl.binding = 0; // expected to be 0 since we only support one vertex buffer
	bindingDecl.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	bindingDecl.stride = (uint32_t)config.stride;

	VkPipelineVertexInputStateCreateInfo vertexInputState{};
	vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputState.vertexBindingDescriptionCount = 1;
	vertexInputState.pVertexBindingDescriptions = &bindingDecl;
	vertexInputState.vertexAttributeDescriptionCount = (uint32_t)vertexDescriptions.size();
	vertexInputState.pVertexAttributeDescriptions = vertexDescriptions.data();


	VkPipelineShaderStageCreateInfo shaderStages[2] = {
		shaderContainer.vertex.value().m_stageInfo, shaderContainer.fragment.value().m_stageInfo
	};

	VkGraphicsPipelineCreateInfo pipelineCI = {};
	pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCI.basePipelineIndex = -1;
	pipelineCI.basePipelineHandle = VK_NULL_HANDLE;
	pipelineCI.layout = pipelineLayout;
	pipelineCI.pInputAssemblyState = &inputAssemblyState;
	pipelineCI.pRasterizationState = &rasterizationState;
	pipelineCI.pColorBlendState = &colorBlendState;
	pipelineCI.pMultisampleState = &multisampleState;
	pipelineCI.pViewportState = &viewportState;
	pipelineCI.pDepthStencilState = &depthStencilState;
	pipelineCI.pDynamicState = &dynamicState;
	pipelineCI.stageCount = 2;
	pipelineCI.pStages = shaderStages;
	pipelineCI.pVertexInputState = &vertexInputState;

	VkFormat depthFormat = renderer->GetDepthTexture()->GetFormat();
	VkFormat colorFormat = renderer->GetCurrentSwapchainFrameTexture()->GetFormat();

	VkPipelineRenderingCreateInfoKHR pipelineRenderingCreateInfo{};
	pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
	pipelineRenderingCreateInfo.colorAttachmentCount = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &colorFormat;
	pipelineRenderingCreateInfo.depthAttachmentFormat = depthFormat;
	pipelineRenderingCreateInfo.stencilAttachmentFormat = depthFormat;
	pipelineRenderingCreateInfo.pNext = nullptr;

	// Chain into the pipeline createinfo
	pipelineCI.pNext = &pipelineRenderingCreateInfo;

	auto device = renderer->GetDevice()->GetLogicalDevice();
	auto allocator = renderer->GetAllocator();

	CR_RETURN( vkCreateGraphicsPipelines( device, VK_NULL_HANDLE, 1, &pipelineCI, allocator, outPipeline ) );
	return VK_SUCCESS;
}

VkResult ShaderCache::CreateComputePipeline( const Renderer* renderer, std::string shaderName, VkPipelineLayout pipelineLayout, VkPipeline* outPipeline )
{
	assert( s_initialized );

	auto it = s_cache.find( shaderName );
	if( it == s_cache.end() )
	{
		Log::Error( "Shader \"%s\" not found in cache", shaderName.c_str() );
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	auto& shaderContainer = it->second;
	if( !shaderContainer.compute.has_value() )
	{
		Log::Error( "Shader \"%s\" is missing a compute shader", shaderName.c_str() );
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	VkComputePipelineCreateInfo computePipelineCreateInfo = {};
	computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo.stage = shaderContainer.compute.value().m_stageInfo;
	computePipelineCreateInfo.layout = pipelineLayout;

	// Create pipeline
	CR_RETURN( vkCreateComputePipelines( renderer->GetDevice()->GetLogicalDevice(), VK_NULL_HANDLE, 1, &computePipelineCreateInfo, renderer->GetAllocator(), outPipeline ) );

	return VK_SUCCESS;
}

std::vector<std::string> ShaderCache::GetAvailableShaderNames( const std::vector<cmf::VertexElement>& availableVertexElements )
{
	std::vector<std::string> result;
	result.reserve( s_cache.size() );

	std::vector<cmf::Usage> elements;
	elements.reserve( availableVertexElements.size() );
	std::transform(
		availableVertexElements.begin(),
		availableVertexElements.end(),
		std::back_inserter( elements ),
		[]( const cmf::VertexElement& element ) {
			return element.usage;
		} );

	std::for_each( s_cache.begin(), s_cache.end(), [&result, &elements]( const auto& keyValue ) {
		ShaderContainer shaderContainer = keyValue.second;
		// only model shaders available
		if( !shaderContainer.isModelShader )
		{
			return;
		}

		// check if there are any inputs that the shader expects that is not in the cmf vertex declaration
		for( const auto& inputLayout : shaderContainer.inputLayout )
		{
			if( std::find( elements.begin(), elements.end(), inputLayout.usage ) == elements.end() )
			{
				return;
			}
		}

		result.push_back( keyValue.first );
	} );
	return result;
}

std::vector<cmf::Usage> ShaderCache::GetShaderUsage( std::string shaderName )
{
	std::vector<cmf::Usage> usage{};
	auto entry = s_cache.find( shaderName );

	if( entry == s_cache.end() )
	{
		return usage;
	}
	auto shaderContainer = ( *entry ).second;
	for( const auto& layout : shaderContainer.inputLayout )
	{
		usage.push_back( layout.usage );
	}
	return usage;
}


void ShaderCache::GenerateVertexDescriptions( std::string shaderName, const std::vector<cmf::VertexElement>& availableVertexElements, std::vector<VkVertexInputAttributeDescription>* outAttributeDescriptions )
{
	outAttributeDescriptions->clear();
	auto entry = s_cache.find( shaderName );

	if( entry == s_cache.end() )
	{
		return;
	}
	auto shaderContainer = ( *entry ).second;
	// create the vulkan vertex input attribute description vector and the vertex input binding description vector based on the shader's expected input layout and the available vertex elements in the model
	for( const auto& layout : shaderContainer.inputLayout )
	{
		auto foundElement = std::find_if( availableVertexElements.begin(), availableVertexElements.end(), [layout]( const cmf::VertexElement& element ) {
			return element.usage == layout.usage;
		} );

		if( foundElement != availableVertexElements.end() )
		{
			auto& e = *foundElement;
			VkVertexInputAttributeDescription decl{};
			decl.format = VulkanEnums::ElementTypeToVkFormat( e.type, e.elementCount );
			decl.offset = e.offset;
			decl.location = layout.location; // use the location from the shader input layout
			decl.binding = 0; // always use a single buffer, until something else is needed
			outAttributeDescriptions->push_back( decl );
		}
	}
}

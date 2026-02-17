#include "shadercache.h"

#include "vulkanerrors.h"

std::map<std::string, std::tuple<std::optional<Shader>, std::optional<Shader>>> ShaderCache::s_cache = {
#include "generatedShaderCache.h"
};
bool ShaderCache::s_initialized = false;

Shader::Shader() :
	m_module( VK_NULL_HANDLE )
{
	Log::Error( "Default Shader constructor called. This is likely an error." );
}

Shader::Shader( std::vector<uint32_t> code ) :
	m_module( VK_NULL_HANDLE )
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
	for( auto& [key, shaderTuple] : s_cache )
	{
		auto& [vertexOpt, fragmentOpt] = shaderTuple;
		if( vertexOpt.has_value() )
		{
			CR_RETURN( vertexOpt.value().Initialize( renderer, VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT ) );
		}
		if( fragmentOpt.has_value() )
		{
			CR_RETURN( fragmentOpt.value().Initialize( renderer, VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT ) );
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

	for( auto& [key, shaderTuple] : s_cache )
	{
		auto& [vertexOpt, fragmentOpt] = shaderTuple;
		if( vertexOpt.has_value() && vertexOpt->m_module != VK_NULL_HANDLE )
		{
			vkDestroyShaderModule( logicalDevice, vertexOpt->m_module, allocator );
		}
		if( fragmentOpt.has_value() && fragmentOpt->m_module != VK_NULL_HANDLE )
		{
			vkDestroyShaderModule( logicalDevice, fragmentOpt->m_module, allocator );
		}
	}
	s_initialized = false;
}

VkResult ShaderCache::CreatePipeline( const Renderer* renderer, std::string shaderName, Effect::Config config, VkPipelineLayout pipelineLayout, VkPipeline* outPipeline )
{
	assert( s_initialized );

	auto it = s_cache.find( shaderName );
	if( it == s_cache.end() )
	{
		Log::Error( "Shader %s not found in cache", shaderName.c_str() );
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	auto& shaderTuple = it->second;
	auto& [vertexOpt, fragmentOpt] = shaderTuple;

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

	VkVertexInputBindingDescription vertexInputBinding{};
	vertexInputBinding.binding = 0;
	vertexInputBinding.stride = config.vertexStride;
	vertexInputBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkPipelineVertexInputStateCreateInfo vertexInputState{};
	vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputState.vertexBindingDescriptionCount = 1;
	vertexInputState.pVertexBindingDescriptions = &vertexInputBinding;
	vertexInputState.vertexAttributeDescriptionCount = (uint32_t)config.vertexDescriptions.size();
	vertexInputState.pVertexAttributeDescriptions = config.vertexDescriptions.data();

	VkPipelineShaderStageCreateInfo shaderStages[2] = {
		vertexOpt.value().m_stageInfo, fragmentOpt.value().m_stageInfo
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

std::vector<std::string> ShaderCache::GetAvailableShaderNames()
{
	std::vector<std::string> result;
	result.reserve( s_cache.size() );
	std::for_each( s_cache.begin(), s_cache.end(), [&result]( const auto& keyValue ) {
		result.push_back( keyValue.first );
	} );
	return result;
}

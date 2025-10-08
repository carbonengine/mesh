#include "shadercache.h"

std::map<std::string, std::tuple<std::optional<Shader>, std::optional<Shader>>> ShaderCache::s_cache = {
#include "generatedShaderCache.h"
};

Shader::Shader() :
	m_module( VK_NULL_HANDLE )
{
	CCP_LOGERR( "Default Shader constructor called. This is likely an error." );
}

Shader::Shader( std::vector<uint32_t> code ) :
	m_module( VK_NULL_HANDLE )
{
	m_code = code;
}

VkResult Shader::Release( const Renderer* renderer )
{
	if( m_module != VK_NULL_HANDLE )
	{
		vkDestroyShaderModule( renderer->GetDevice()->GetLogicalDevice(), m_module, renderer->GetAllocator() );
		m_module = VK_NULL_HANDLE;
	}
	return VK_SUCCESS;
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

ShaderCache::ShaderCache()
{
}

VkResult ShaderCache::Release( const Renderer* renderer )
{
	for( auto& [key, shaderTuple] : s_cache )
	{
		auto& [vertexOpt, fragmentOpt] = shaderTuple;
		if( vertexOpt.has_value() )
		{
			vertexOpt.value().Release( renderer );
		}
		if( fragmentOpt.has_value() )
		{
			fragmentOpt.value().Release( renderer );
		}
	}
	auto logical = renderer->GetDevice()->GetLogicalDevice();
	auto allocator = renderer->GetAllocator();
	vkDestroyDescriptorSetLayout( logical, m_descriptorSetLayout, allocator );
	vkDestroyPipelineLayout( logical, m_pipelineLayout, allocator );
	return VK_SUCCESS;
}

VkResult ShaderCache::Initialize( const Renderer* renderer )
{
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
	CreatePipelineLayout( renderer );
	return VK_SUCCESS;
}

VkResult ShaderCache::CreatePipelineLayout( const Renderer* renderer )
{
	auto device = renderer->GetDevice()->GetLogicalDevice();

	// Binding 0: Uniform buffer (Vertex shader)
	VkDescriptorSetLayoutBinding layoutBinding{};
	layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	layoutBinding.descriptorCount = 1;
	layoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	layoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo descriptorLayoutCI{};
	descriptorLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorLayoutCI.pNext = nullptr;
	descriptorLayoutCI.bindingCount = 1;
	descriptorLayoutCI.pBindings = &layoutBinding;
	RETURN_LOG_ERROR( vkCreateDescriptorSetLayout( device, &descriptorLayoutCI, nullptr, &m_descriptorSetLayout ), "Failed to create descriptor set layout" );

	std::vector<VkDescriptorSetLayout> setLayouts = { m_descriptorSetLayout };
	VkPipelineLayoutCreateInfo pipelineLayoutCI{};
	pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCI.setLayoutCount = static_cast<uint32_t>( setLayouts.size() );
	pipelineLayoutCI.pSetLayouts = setLayouts.data();
	pipelineLayoutCI.pushConstantRangeCount = 0;
	pipelineLayoutCI.pPushConstantRanges = nullptr;
	RETURN_LOG_ERROR( vkCreatePipelineLayout( device, &pipelineLayoutCI, nullptr, &m_pipelineLayout ), "Failed to create pipeline layout" );
	return VK_SUCCESS;
}

VkResult ShaderCache::CreatePipeline( const Renderer* renderer, std::string shaderName, VkPolygonMode mode, uint32_t stride, std::vector<VkVertexInputAttributeDescription> vertexDescriptions, VkPipeline* outPipeline )
{
	auto it = s_cache.find( shaderName );
	if( it == s_cache.end() )
	{
		CCP_LOGERR( "Shader %s not found in cache", shaderName.c_str() );
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	auto& shaderTuple = it->second;
	auto& [vertexOpt, fragmentOpt] = shaderTuple;

	VkVertexInputBindingDescription vertexInputBinding{};
	vertexInputBinding.binding = 0;
	vertexInputBinding.stride = stride;
	vertexInputBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkPipelineVertexInputStateCreateInfo vertexInputStateCI{};
	vertexInputStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateCI.vertexBindingDescriptionCount = 1;
	vertexInputStateCI.pVertexBindingDescriptions = &vertexInputBinding;
	vertexInputStateCI.vertexAttributeDescriptionCount = (uint32_t)vertexDescriptions.size();
	vertexInputStateCI.pVertexAttributeDescriptions = vertexDescriptions.data();

	// input assembly
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI{};
	inputAssemblyStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyStateCI.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	// Rasterization state
	VkPipelineRasterizationStateCreateInfo rasterizationStateCI{};
	rasterizationStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateCI.polygonMode = mode;
	rasterizationStateCI.cullMode = mode == VK_POLYGON_MODE_FILL ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
	rasterizationStateCI.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationStateCI.depthClampEnable = VK_FALSE;
	rasterizationStateCI.rasterizerDiscardEnable = VK_FALSE;
	rasterizationStateCI.depthBiasEnable = VK_FALSE;
	rasterizationStateCI.lineWidth = 1.0f;

	// blend state
	VkPipelineColorBlendAttachmentState blendAttachmentState{};
	blendAttachmentState.colorWriteMask = 0xf;
	blendAttachmentState.blendEnable = VK_FALSE;
	VkPipelineColorBlendStateCreateInfo colorBlendStateCI{};
	colorBlendStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendStateCI.attachmentCount = 1;
	colorBlendStateCI.pAttachments = &blendAttachmentState;

	// viewport states
	VkPipelineViewportStateCreateInfo viewportStateCI{};
	viewportStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCI.viewportCount = 1;
	viewportStateCI.scissorCount = 1;

	// Enable dynamic states for viewport and scissor
	std::vector<VkDynamicState> dynamicStateEnables;
	dynamicStateEnables.push_back( VK_DYNAMIC_STATE_VIEWPORT );
	dynamicStateEnables.push_back( VK_DYNAMIC_STATE_SCISSOR );
	VkPipelineDynamicStateCreateInfo dynamicStateCI{};
	dynamicStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCI.pDynamicStates = dynamicStateEnables.data();
	dynamicStateCI.dynamicStateCount = static_cast<uint32_t>( dynamicStateEnables.size() );

	// depth stencil
	VkPipelineDepthStencilStateCreateInfo depthStencilStateCI{};
	depthStencilStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilStateCI.depthTestEnable = VK_TRUE;
	depthStencilStateCI.depthWriteEnable = VK_TRUE;
	depthStencilStateCI.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencilStateCI.depthBoundsTestEnable = VK_FALSE;
	depthStencilStateCI.back.failOp = VK_STENCIL_OP_KEEP;
	depthStencilStateCI.back.passOp = VK_STENCIL_OP_KEEP;
	depthStencilStateCI.back.compareOp = VK_COMPARE_OP_ALWAYS;
	depthStencilStateCI.stencilTestEnable = VK_FALSE;
	depthStencilStateCI.front = depthStencilStateCI.back;

	VkGraphicsPipelineCreateInfo pipelineCI{};
	pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	// The layout used for this pipeline (can be shared among multiple pipelines using the same layout)
	pipelineCI.layout = m_pipelineLayout;
	// Renderpass this pipeline is attached to
	pipelineCI.renderPass = renderer->GetRenderPass();

	// No multisampling
	VkPipelineMultisampleStateCreateInfo multisampleStateCI{};
	multisampleStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleStateCI.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleStateCI.pSampleMask = nullptr;

	// Set pipeline shader stage info
	VkPipelineShaderStageCreateInfo shaderStages[2] = {
		vertexOpt.value().m_stageInfo, fragmentOpt.value().m_stageInfo
	};
	pipelineCI.stageCount = 2;
	pipelineCI.pStages = shaderStages;

	// Assign the pipeline states to the pipeline creation info structure
	pipelineCI.pVertexInputState = &vertexInputStateCI;
	pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
	pipelineCI.pRasterizationState = &rasterizationStateCI;
	pipelineCI.pColorBlendState = &colorBlendStateCI;
	pipelineCI.pMultisampleState = &multisampleStateCI;
	pipelineCI.pViewportState = &viewportStateCI;
	pipelineCI.pDepthStencilState = &depthStencilStateCI;
	pipelineCI.pDynamicState = &dynamicStateCI;

	static VkPipelineCache pipelineCache;

	VkPipelineCacheCreateInfo pipelineCacheCreateInfo;
	pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	CR_RETURN( vkCreateGraphicsPipelines( renderer->GetDevice()->GetLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineCI, nullptr, outPipeline ) );

	return VK_SUCCESS;
}

VkPipelineLayout ShaderCache::GetPipelineLayout() const
{
	return m_pipelineLayout;
}

VkDescriptorSetLayout ShaderCache::GetDescriptorSetLayout() const
{
	return m_descriptorSetLayout;
}
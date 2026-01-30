#include "shadercache.h"

#include "vulkanerrors.h"

std::map<std::string, std::tuple<std::optional<Shader>, std::optional<Shader>>> ShaderCache::s_cache = {
#include "generatedShaderCache.h"
};

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

ShaderCache::ShaderCache( std::shared_ptr<Renderer> renderer ) :
	m_renderer( renderer )
{
}

ShaderCache::~ShaderCache()
{
	for( auto& [key, shaderTuple] : s_cache )
	{
		auto& [vertexOpt, fragmentOpt] = shaderTuple;
		if( vertexOpt.has_value() )
		{
			vertexOpt.value().Release( m_renderer.get() );
		}
		if( fragmentOpt.has_value() )
		{
			fragmentOpt.value().Release( m_renderer.get() );
		}
	}
	auto logical = m_renderer->GetDevice()->GetLogicalDevice();
	auto allocator = m_renderer->GetAllocator();
	vkDestroyDescriptorSetLayout( logical, m_descriptorSetLayout, allocator );
	vkDestroyPipelineLayout( logical, m_pipelineLayout, allocator );
}

VkResult ShaderCache::Initialize()
{
	for( auto& [key, shaderTuple] : s_cache )
	{
		auto& [vertexOpt, fragmentOpt] = shaderTuple;
		if( vertexOpt.has_value() )
		{
			CR_RETURN( vertexOpt.value().Initialize( m_renderer.get(), VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT ) );
		}
		if( fragmentOpt.has_value() )
		{
			CR_RETURN( fragmentOpt.value().Initialize( m_renderer.get(), VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT ) );
		}
	}
	CreatePipelineLayout();
	return VK_SUCCESS;
}

VkResult ShaderCache::CreatePipelineLayout()
{
	auto device = m_renderer->GetDevice()->GetLogicalDevice();

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

VkResult ShaderCache::CreatePipeline( std::string shaderName, VkPrimitiveTopology topology, VkPolygonMode mode, float lineWidth, uint32_t stride, std::vector<VkVertexInputAttributeDescription> vertexDescriptions, VkPipeline* outPipeline ) const
{
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
	inputAssemblyState.topology = topology;

	VkPipelineRasterizationStateCreateInfo rasterizationState{};
	rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationState.polygonMode = mode;
	rasterizationState.cullMode = mode == VK_POLYGON_MODE_FILL ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
	rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationState.depthClampEnable = VK_FALSE;
	rasterizationState.lineWidth = lineWidth;

	VkPipelineColorBlendAttachmentState blendAttachmentState{};
	blendAttachmentState.colorWriteMask = 0xf;
	blendAttachmentState.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo colorBlendState{};
	colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendState.attachmentCount = 1;
	colorBlendState.pAttachments = &blendAttachmentState;

	VkPipelineDepthStencilStateCreateInfo depthStencilState{};
	depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilState.depthTestEnable = VK_TRUE;
	depthStencilState.depthWriteEnable = VK_TRUE;
	depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
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

	std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.pDynamicStates = &dynamicStateEnables[0];
	dynamicState.dynamicStateCount = static_cast<uint32_t>( dynamicStateEnables.size() );
	dynamicState.flags = 0;

	VkVertexInputBindingDescription vertexInputBinding{};
	vertexInputBinding.binding = 0;
	vertexInputBinding.stride = stride;
	vertexInputBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkPipelineVertexInputStateCreateInfo vertexInputState{};
	vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputState.vertexBindingDescriptionCount = 1;
	vertexInputState.pVertexBindingDescriptions = &vertexInputBinding;
	vertexInputState.vertexAttributeDescriptionCount = (uint32_t)vertexDescriptions.size();
	vertexInputState.pVertexAttributeDescriptions = vertexDescriptions.data();

	VkPipelineShaderStageCreateInfo shaderStages[2] = {
		vertexOpt.value().m_stageInfo, fragmentOpt.value().m_stageInfo
	};

	VkGraphicsPipelineCreateInfo pipelineCI = {};
	pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCI.basePipelineIndex = -1;
	pipelineCI.basePipelineHandle = VK_NULL_HANDLE;
	pipelineCI.layout = m_pipelineLayout;
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

	VkFormat depthFormat = m_renderer->GetDepthTexture()->GetFormat();
	VkFormat colorFormat = m_renderer->GetCurrentSwapchainFrameTexture()->GetFormat();

	// New create info to define color, depth and stencil attachments at pipeline create time
	VkPipelineRenderingCreateInfoKHR pipelineRenderingCreateInfo{};
	pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
	pipelineRenderingCreateInfo.colorAttachmentCount = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &colorFormat;
	pipelineRenderingCreateInfo.depthAttachmentFormat = depthFormat;
	pipelineRenderingCreateInfo.stencilAttachmentFormat = depthFormat;
	pipelineRenderingCreateInfo.pNext = nullptr;

	// Chain into the pipeline createinfo
	pipelineCI.pNext = &pipelineRenderingCreateInfo;

	auto device = m_renderer->GetDevice()->GetLogicalDevice();
	auto allocator = m_renderer->GetAllocator();

	CR_RETURN( vkCreateGraphicsPipelines( device, VK_NULL_HANDLE, 1, &pipelineCI, allocator, outPipeline ) );
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

std::vector<std::string> ShaderCache::GetAvailableShaderNames()
{
	std::vector<std::string> result;
	result.reserve( s_cache.size() );
	std::for_each( s_cache.begin(), s_cache.end(), [&result]( const auto& keyValue ) {
		result.push_back( keyValue.first );
	} );
	return result;
}

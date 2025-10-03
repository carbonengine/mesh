#include "model.h"
#include "../effect/effect.h"
#include <cmf/utils.h>

namespace ModelLoader
{
Model* LoadModelFromFile( const std::string& filePath )
{
	CCP_LOGNOTICE( "Loading model from file: %s", filePath.c_str() );

	// read the file and cast it to m_cmfModel
	const char* filename = filePath.c_str();
	const char* mode = "rb";

	FILE* file = nullptr;
	fopen_s( &file, filename, mode );

	if( !file )
	{
		CCP_LOGERR( "Failed to open file: %s", filename );
		return nullptr;
	}

	fseek( file, 0, SEEK_END );
	size_t fileSize = ftell( file );
	fseek( file, 0, SEEK_SET );
	std::vector<uint8_t> fileData( fileSize );
	size_t bytesRead = fread( fileData.data(), 1, fileSize, file );
	if( bytesRead != fileSize )
	{
		CCP_LOGERR( "Failed to read file: %s", filename );
		fclose( file );
		return nullptr;
	}
	fclose( file );

	auto validationResult = cmf::ValidateFile( fileData.data(), fileData.size(), { true, true, true } );
	if( !validationResult.first )
	{
		CCP_LOGERR( "File validation failed: %s", filename );
		return nullptr;
	}
	if( !validationResult.second.validateHeader )
	{
		CCP_LOGERR( "File header validation failed: %s", filename );
		return nullptr;
	}
	if( !validationResult.second.validateMainData )
	{
		CCP_LOGERR( "File main data validation failed: %s", filename );
		return nullptr;
	}

	// cast the data to m_cmfModel
	return new Model( fileData, filePath );
}
};

VkFormat ConvertElementType( cmf::ElementType element, uint8_t count )
{
	switch( element )
	{
	case cmf::ElementType::Float32:

		if( count == 4 )
		{
			return VK_FORMAT_R32G32B32A32_SFLOAT;
		}
		else if( count == 3 )
		{
			return VK_FORMAT_R32G32B32_SFLOAT;
		}
		else if( count == 2 )
		{
			return VK_FORMAT_R32G32_SFLOAT;
		}
		else if( count == 1 )
		{
			return VK_FORMAT_R32_SFLOAT;
		}
		break;
	case cmf::ElementType::Float16:
		if( count == 4 )
		{
			return VK_FORMAT_R16G16B16A16_SFLOAT;
		}
		else if( count == 3 )
		{
			return VK_FORMAT_R16G16B16_SFLOAT;
		}
		else if( count == 2 )
		{
			return VK_FORMAT_R16G16_SFLOAT;
		}
		else
		{
			return VK_FORMAT_R16_SFLOAT;
		}
		break;
	case cmf::ElementType::UInt16Norm:
		if( count == 4 )
		{
			return VK_FORMAT_R16G16B16A16_UNORM;
		}
		else if( count == 3 )
		{
			return VK_FORMAT_R16G16B16_UNORM;
		}
		else if( count == 2 )
		{
			return VK_FORMAT_R16G16_UNORM;
		}
		else if( count == 1 )
		{
			return VK_FORMAT_R16_UNORM;
		}
		break;
	case cmf::ElementType::UInt16:
		if( count == 4 )
		{
			return VK_FORMAT_R16G16B16A16_UINT;
		}
		else if( count == 3 )
		{
			return VK_FORMAT_R16G16B16_UINT;
		}
		else if( count == 2 )
		{
			return VK_FORMAT_R16G16_UINT;
		}
		else if( count == 1 )
		{
			return VK_FORMAT_R16_UINT;
		}
		break;
	case cmf::ElementType::Int16Norm:
		if( count == 4 )
		{
			return VK_FORMAT_R16G16B16A16_SNORM;
		}
		else if( count == 3 )
		{
			return VK_FORMAT_R16G16B16_SNORM;
		}
		else if( count == 2 )
		{
			return VK_FORMAT_R16G16_SNORM;
		}
		else if( count == 1 )
		{
			return VK_FORMAT_R16_SNORM;
		}
		break;
	case cmf::ElementType::Int16:
		if( count == 4 )
		{
			return VK_FORMAT_R16G16B16A16_SINT;
		}
		else if( count == 3 )
		{
			return VK_FORMAT_R16G16B16_SINT;
		}
		else if( count == 2 )
		{
			return VK_FORMAT_R16G16_SINT;
		}
		else if( count == 1 )
		{
			return VK_FORMAT_R16_SINT;
		}
		break;
	case cmf::ElementType::UInt8Norm:
		if( count == 4 )
		{
			return VK_FORMAT_R8G8B8A8_UNORM;
		}
		else if( count == 3 )
		{
			return VK_FORMAT_R8G8B8_UNORM;
		}
		else if( count == 2 )
		{
			return VK_FORMAT_R8G8_UNORM;
		}
		else if( count == 1 )
		{
			return VK_FORMAT_R8_UNORM;
		}
		break;

	case cmf::ElementType::UInt8:
		if( count == 4 )
		{
			return VK_FORMAT_R8G8B8A8_UINT;
		}
		else if( count == 3 )
		{
			return VK_FORMAT_R8G8B8_UINT;
		}
		else if( count == 2 )
		{
			return VK_FORMAT_R8G8_UINT;
		}
		else if( count == 1 )
		{
			return VK_FORMAT_R8_UINT;
		}
		break;
	case cmf::ElementType::Int8Norm:
		if( count == 4 )
		{
			return VK_FORMAT_R8G8B8A8_SNORM;
		}
		else if( count == 3 )
		{
			return VK_FORMAT_R8G8B8_SNORM;
		}
		else if( count == 2 )
		{
			return VK_FORMAT_R8G8_SNORM;
		}
		else if( count == 1 )
		{
			return VK_FORMAT_R8_SNORM;
		}
		break;

	case cmf::ElementType::Int8:
		if( count == 4 )
		{
			return VK_FORMAT_R8G8B8A8_SINT;
		}
		else if( count == 3 )
		{
			return VK_FORMAT_R8G8B8_SINT;
		}
		else if( count == 2 )
		{
			return VK_FORMAT_R8G8_SINT;
		}
		else if( count == 1 )
		{
			return VK_FORMAT_R8_SINT;
		}
		break;
	}
	return VK_FORMAT_UNDEFINED;
}

Model::Model() :
	m_cmfData( nullptr ),
	m_cmfHeader( nullptr ),
	m_effect( nullptr ),
	m_pipelineLayout( VK_NULL_HANDLE )
{
}

Model::Model( std::vector<uint8_t> fileContent, std::string filePath ) :
	m_effect( nullptr ),
	m_pipelineLayout( VK_NULL_HANDLE )
{
	m_filePath = filePath;
	m_fileContent = fileContent;
	if( m_fileContent.size() != 0 )
	{
		auto data = m_fileContent.data();
		m_cmfHeader = reinterpret_cast<cmf::Header*>( data );
		cmf::OffsetsToPointers( m_cmfHeader );

		m_cmfData = reinterpret_cast<cmf::Data*>( data + m_cmfHeader->sections[0].offset );
		cmf::OffsetsToPointers( m_cmfData );
	}
}

Model::~Model()
{
	if( m_cmfHeader )
	{
		delete m_cmfHeader;
	}
	if( m_cmfData )
	{
		delete m_cmfData;
	}
}

void Model::Release( Device* device )
{
	for( auto& mesh : m_meshes )
	{
		for( auto& lod : mesh.lods )
		{
			lod.vertexBuffer->Release( device );
			if( lod.indexBuffer )
			{
				lod.indexBuffer->Release( device );
			}
		}

		for( auto& pipeline : mesh.pipelines )
		{
			vkDestroyPipeline( device->GetLogicalDevice(), pipeline, nullptr );
		}
		mesh.pipelines.clear();
	}
	delete m_cmfHeader;
	delete m_cmfData;
}


VkResult Model::Initialize( Device* device, VkCommandPool commandPool, VkRenderPass renderPass, VkDescriptorSetLayout descriptorSetLayout )
{
	RETURN_ERROR( SetupBuffers( device, commandPool ) );
	m_effect = EffectLoader::LoadEffectFromFile( device, "test.vert.spv", "test.frag.spv" );
	RETURN_ERROR( SetupPipeline( device, renderPass, descriptorSetLayout ) );
	CCP_LOGNOTICE( "Model %s initialized successfully", m_filePath.c_str() );
	return VK_SUCCESS;
}

VkResult Model::Render( VkCommandBuffer commandBuffer, VkDescriptorSet perFrameDataDescriptor, size_t meshIndex, size_t lodIndex )
{
	if( meshIndex >= m_meshes.size() )
	{
		CCP_LOGERR( "Mesh index out of range" );
		return VK_ERROR_INITIALIZATION_FAILED;
	}
	auto& mesh = m_meshes[meshIndex];
	if( lodIndex >= mesh.lods.size() )
	{
		CCP_LOGERR( "Lod index out of range" );
		return VK_ERROR_INITIALIZATION_FAILED;
	}
	auto& lod = mesh.lods[lodIndex];
	vkCmdBindDescriptorSets( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &perFrameDataDescriptor, 0, nullptr );

	vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh.pipelines[0] );
	VkDeviceSize offsets[] = { 0 };

	auto vertexBuffer = lod.vertexBuffer->GetGpuBuffer();
	vkCmdBindVertexBuffers( commandBuffer, 0, 1, &vertexBuffer, offsets );
	if( lod.indexBuffer )
	{
		auto indexBuffer = lod.indexBuffer->GetGpuBuffer();

		auto indexBufferType = lod.indexStride == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;

		vkCmdBindIndexBuffer( commandBuffer, indexBuffer, 0, indexBufferType );
		vkCmdDrawIndexed( commandBuffer, lod.indexBuffer->size() / lod.indexStride, 1, 0, 0, 0 );
	}
	else
	{
		vkCmdDraw( commandBuffer, lod.vertexBuffer->size() / lod.vertexStride, 1, 0, 0 );
	}
	return VK_SUCCESS;
}

VkResult Model::SetupBuffers( Device* device, VkCommandPool commandPool )
{
	// finalize the copying
	auto logical = device->GetLogicalDevice();
	// Buffer copies have to be submitted to a queue, so we need a command buffer for them
	// Note: Some devices offer a dedicated transfer queue (with only the transfer bit set) that may be faster when doing lots of copies
	VkCommandBuffer copyCmd;

	VkCommandBufferAllocateInfo cmdBufAllocateInfo{};
	cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBufAllocateInfo.commandPool = commandPool;
	cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdBufAllocateInfo.commandBufferCount = 1;
	CR_RETURN( vkAllocateCommandBuffers( logical, &cmdBufAllocateInfo, &copyCmd ) );

	VkCommandBufferBeginInfo cmdBufInfo{};
	cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	CR_RETURN( vkBeginCommandBuffer( copyCmd, &cmdBufInfo ) );

	uint32_t meshIndex = 0;

	// create all the buffers
	for( const auto& cmfMesh : m_cmfData->meshes )
	{
		auto mesh = Mesh{};
		uint8_t i = 0;
		for( const auto& decl : cmfMesh.decl )
		{
			VkVertexInputAttributeDescription attrDesc{};
			attrDesc.binding = 0;
			attrDesc.location = i++;
			attrDesc.offset = decl.offset;
			attrDesc.format = ConvertElementType( decl.type, decl.elementCount );

			mesh.vertexDescriptions.push_back( attrDesc );
		}

		uint32_t lodIndex = 0;
		for( const auto& lod : cmfMesh.lods )
		{
			ModelLod modelLod{};
			uint32_t offset = m_cmfHeader->sections[lod.vb.index].offset + lod.vb.offset;

			modelLod.vertexBuffer = BufferBuilder::Build( device, m_fileContent.data(), BufferTypeVertex, lod.vb.size, offset, lod.vb.stride, lod.vb.index );
			if( modelLod.vertexBuffer == nullptr )
			{
				CCP_LOGERR( "Failed to create vertex buffer for mesh %d lod %d", meshIndex, lodIndex );
				continue;
			}
			modelLod.vertexBuffer->CopyFromStaging( copyCmd );
			modelLod.vertexStride = lod.vb.stride;

			if( lod.ib.size > 0 )
			{
				uint32_t offset = m_cmfHeader->sections[lod.ib.index].offset + lod.ib.offset;

				modelLod.indexBuffer = BufferBuilder::Build( device, m_fileContent.data(), BufferTypeIndex, lod.ib.size, offset, lod.ib.stride, lod.ib.index );
				if( modelLod.indexBuffer == nullptr )
				{
					CCP_LOGERR( "Failed to create index buffer for mesh %d lod %d", meshIndex, lodIndex );
					continue;
				}
				modelLod.indexBuffer->CopyFromStaging( copyCmd );
				modelLod.indexStride = lod.ib.stride;
			}
			mesh.lods.push_back( modelLod );
			lodIndex++;
		}
		m_meshes.push_back( mesh );
		meshIndex++;
	}

	CR_RETURN( vkEndCommandBuffer( copyCmd ) );

	// Submit the command buffer to the queue to finish the copy
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &copyCmd;

	// Create fence to ensure that the command buffer has finished executing
	VkFenceCreateInfo fenceCI{};
	fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCI.flags = 0;
	VkFence fence;
	CR_RETURN( vkCreateFence( logical, &fenceCI, nullptr, &fence ) );

	// Submit to the queue
	CR_RETURN( vkQueueSubmit( device->GetGraphicsQueue(), 1, &submitInfo, fence ) );
	// Wait for the fence to signal that command buffer has finished executing
	CR_RETURN( vkWaitForFences( logical, 1, &fence, VK_TRUE, 100000000000 ) );

	vkDestroyFence( logical, fence, nullptr );
	vkFreeCommandBuffers( logical, commandPool, 1, &copyCmd );

	for( auto& mesh : m_meshes )
	{
		for( auto& lod : mesh.lods )
		{
			lod.vertexBuffer->ReleaseStaging( device );
			lod.indexBuffer->ReleaseStaging( device );
		}
	}

	return VK_SUCCESS;
}

VkResult Model::CreatePipelineLayout( VkDevice device, VkDescriptorSetLayout descriptorSetLayout )
{
	std::vector<VkDescriptorSetLayout> setLayouts = { descriptorSetLayout };
	VkPipelineLayoutCreateInfo pipelineLayoutCI{};
	pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCI.setLayoutCount = static_cast<uint32_t>( setLayouts.size() );
	pipelineLayoutCI.pSetLayouts = setLayouts.data();
	pipelineLayoutCI.pushConstantRangeCount = 0;
	pipelineLayoutCI.pPushConstantRanges = nullptr;
	RETURN_LOG_ERROR( vkCreatePipelineLayout( device, &pipelineLayoutCI, nullptr, &m_pipelineLayout ), "Failed to create pipeline layout" );
	return VK_SUCCESS;
}

// Adds pipelines for all the meshes in the model
VkResult Model::SetupPipeline( Device* device, VkRenderPass renderPass, VkDescriptorSetLayout descriptorSetLayout )
{
	if( !m_cmfData )
	{
		CCP_LOGERR( "No model data to create pipeline from" );
	}
	auto logical = device->GetLogicalDevice();

	size_t index = 0;

	CR_RETURN( CreatePipelineLayout( logical, descriptorSetLayout ) );

	for( auto& cmfMesh : m_cmfData->meshes )
	{
		Mesh& modelMesh = m_meshes[index++];
		auto stride = cmfMesh.lods[0].vb.stride;

		VkVertexInputBindingDescription vertexInputBinding{};
		vertexInputBinding.binding = 0;
		vertexInputBinding.stride = stride;
		vertexInputBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		VkPipelineVertexInputStateCreateInfo vertexInputStateCI{};
		vertexInputStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputStateCI.vertexBindingDescriptionCount = 1;
		vertexInputStateCI.pVertexBindingDescriptions = &vertexInputBinding;
		vertexInputStateCI.vertexAttributeDescriptionCount = (uint32_t)modelMesh.vertexDescriptions.size();
		vertexInputStateCI.pVertexAttributeDescriptions = modelMesh.vertexDescriptions.data();

		switch( cmfMesh.topology )
		{
		case cmf::MeshTopology::PointList:
			RETURN_ERROR( FinalizePipeline( modelMesh, logical, renderPass, vertexInputStateCI, VK_POLYGON_MODE_POINT ) );
			break;
		case cmf::MeshTopology::TriangleList:
			RETURN_ERROR( FinalizePipeline( modelMesh, logical, renderPass, vertexInputStateCI, VK_POLYGON_MODE_LINE ) );
			//RETURN_ERROR( FinalizePipeline( modelMesh, logical, renderPass, vertexInputStateCI, VK_POLYGON_MODE_FILL ) );
			break;
		}
		break;
	}

	return VK_SUCCESS;
}

VkResult Model::FinalizePipeline( Mesh& mesh, VkDevice device, VkRenderPass renderPass, VkPipelineVertexInputStateCreateInfo inputState, VkPolygonMode mode )
{
	// input assembly
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI{};
	inputAssemblyStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyStateCI.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	// Rasterization state
	VkPipelineRasterizationStateCreateInfo rasterizationStateCI{};
	rasterizationStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateCI.polygonMode = mode;
	rasterizationStateCI.cullMode = VK_CULL_MODE_NONE; //mode == VK_POLYGON_MODE_FILL ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
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
	pipelineCI.renderPass = renderPass;

	// No multisampling
	VkPipelineMultisampleStateCreateInfo multisampleStateCI{};
	multisampleStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleStateCI.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleStateCI.pSampleMask = nullptr;

	// Set pipeline shader stage info
	auto shaderStages = m_effect->GetShaderStages();
	pipelineCI.stageCount = static_cast<uint32_t>( shaderStages.size() );
	pipelineCI.pStages = shaderStages.data();

	// Assign the pipeline states to the pipeline creation info structure
	pipelineCI.pVertexInputState = &inputState;
	pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
	pipelineCI.pRasterizationState = &rasterizationStateCI;
	pipelineCI.pColorBlendState = &colorBlendStateCI;
	pipelineCI.pMultisampleState = &multisampleStateCI;
	pipelineCI.pViewportState = &viewportStateCI;
	pipelineCI.pDepthStencilState = &depthStencilStateCI;
	pipelineCI.pDynamicState = &dynamicStateCI;
	VkPipeline pipeline = { VK_NULL_HANDLE };

	static VkPipelineCache pipelineCache;

	VkPipelineCacheCreateInfo pipelineCacheCreateInfo;
	pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	CR_RETURN( vkCreateGraphicsPipelines( device, pipelineCache, 1, &pipelineCI, nullptr, &pipeline ) );

	mesh.pipelines.push_back( pipeline );

	return VK_SUCCESS;
}

CcpMath::Sphere Model::GetBoundingSphere() const
{
	// add up all the bounding boxes and create a sphere
	CcpMath::AxisAlignedBox accumulated;
	for( const auto& mesh : m_cmfData->meshes )
	{
		accumulated.IncludeBox( mesh.bounds );
	}

	return CcpMath::Sphere( accumulated );
}

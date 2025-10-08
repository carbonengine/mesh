#include "modelRenderer.h"
#include "vulkan/vulkanenums.h"

ModelRenderer::ModelRenderer() :
	m_pipelineLayout( VK_NULL_HANDLE ),
	m_shaderName( "" )
{
}

ModelRenderer::~ModelRenderer()
{
}

void ModelRenderer::Initialize( const Renderer* renderer )
{
	auto device = renderer->GetDevice();
    auto logicalDevice = device->GetLogicalDevice();
    VkMemoryRequirements memReqs;
	
	// Vertex shader uniform buffer block
	VkBufferCreateInfo bufferInfo{};
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;
	allocInfo.allocationSize = 0;
	allocInfo.memoryTypeIndex = 0;
	
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = sizeof( PerFrameData );
	// This buffer will be used as a uniform buffer
	bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

	ON_ERROR_LOG_AND_RETURN( m_shaderCache.Initialize( renderer ), "Failed to initialize shader cache for model renderer" );
	
    auto descriptorSetLayout = m_shaderCache.GetDescriptorSetLayout();

	VkDescriptorSetAllocateInfo descriptorSetAllocInfo{};
	descriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetAllocInfo.descriptorPool = renderer->GetDescriptorPool();
	descriptorSetAllocInfo.descriptorSetCount = 1;
	descriptorSetAllocInfo.pSetLayouts = &descriptorSetLayout;

	// Create the buffers
	for( uint32_t i = 0; i < RenderUtils::MAX_FRAMES_IN_FLIGHT; i++ )
	{

		ON_ERROR_LOG_AND_RETURN( vkCreateBuffer( logicalDevice, &bufferInfo, nullptr, &m_perFrameBuffers[i].buffer ), "Failed to create perframe buffer" );
		vkGetBufferMemoryRequirements( logicalDevice, m_perFrameBuffers[i].buffer, &memReqs );
		allocInfo.allocationSize = memReqs.size;
		allocInfo.memoryTypeIndex = device->GetMemoryTypeIndex( memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		ON_ERROR_LOG_AND_RETURN( vkAllocateMemory( logicalDevice, &allocInfo, nullptr, &( m_perFrameBuffers[i].memory ) ), "Failed to allocate memory for perframe buffer" );
		ON_ERROR_LOG_AND_RETURN( vkBindBufferMemory( logicalDevice, m_perFrameBuffers[i].buffer, m_perFrameBuffers[i].memory, 0 ), "Failed to bind perframe buffer memory" );
		ON_ERROR_LOG_AND_RETURN( vkMapMemory( logicalDevice, m_perFrameBuffers[i].memory, 0, sizeof( PerFrameData ), 0, (void**)&m_perFrameBuffers[i].mapped ), "Failed to map perframe buffer memory" );

		ON_ERROR_LOG_AND_RETURN( vkAllocateDescriptorSets( logicalDevice, &descriptorSetAllocInfo, &m_perFrameBuffers[i].descriptorSet ), "Failed to allocate descriptor sets" );

		// Update the descriptor set determining the shader binding points
		// For every binding point used in a shader there needs to be one
		// descriptor set matching that binding point
		VkWriteDescriptorSet writeDescriptorSet{};

		// The buffer's information is passed using a descriptor info structure
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = m_perFrameBuffers[i].buffer;
		bufferInfo.range = sizeof( PerFrameData );

		// Binding 0 : Uniform buffer
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstSet = m_perFrameBuffers[i].descriptorSet;
		writeDescriptorSet.descriptorCount = 1;
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeDescriptorSet.pBufferInfo = &bufferInfo;
		writeDescriptorSet.dstBinding = 0;
		vkUpdateDescriptorSets( logicalDevice, 1, &writeDescriptorSet, 0, nullptr );
    }

}

void ModelRenderer::Release( const Renderer* renderer )
{
	m_shaderCache.Release( renderer );

    ReleaseMeshes( renderer );
}

void ModelRenderer::ReleaseMeshes( const Renderer* renderer )
{
	auto logicalDevice = renderer->GetDevice()->GetLogicalDevice();
	auto allocator = renderer->GetAllocator();
	for( auto& mesh : m_meshes )
	{
		for( auto& lod : mesh.lods )
		{
			lod.vertexBuffer->Release( renderer );
			if( lod.indexBuffer )
			{
				lod.indexBuffer->Release( renderer );
			}
		}
		vkDestroyPipeline( logicalDevice, mesh.pipeline, allocator );
	}

	m_meshes.clear();
}

VkResult ModelRenderer::SetPerFrameData( const Renderer* renderer )
{
    if( m_meshes.size() == 0 )
    {
        return VK_SUCCESS;
    }
    auto frameIndex = renderer->GetCurrentFrame();
    // Update uniform buffer
    PerFrameData frameData{};
	memcpy( m_perFrameBuffers[frameIndex].mapped, &frameData, sizeof( PerFrameData ) );
    return VK_SUCCESS;
}

VkResult ModelRenderer::RenderMesh( const Renderer* renderer, size_t meshIndex, size_t lodIndex )
{
	if( m_meshes.size() == 0 )
	{   
        return VK_SUCCESS;
    }

    if( meshIndex >= m_meshes.size() )
    {
		CCP_LOGERR( "Mesh index out of range" );
        return VK_SUCCESS;
    }
    auto& mesh = m_meshes[meshIndex];
    if( lodIndex >= mesh.lods.size() )
    {
		CCP_LOGERR( "Mesh index out of range" );
		return VK_SUCCESS;
	}

    auto& lod = mesh.lods[lodIndex];
	auto commandBuffer = renderer->GetCurrentCommandBuffer();
	auto vertexBuffer = lod.vertexBuffer->GetGpuBuffer();
    VkDeviceSize offsets[] = { 0 };

    // Update the perframe data
	PerFrameData perframe{};
	perframe.proj = PerspectiveFovMatrix( 1.0f, (float)renderer->GetWidth() / (float)renderer->GetHeight(), 0.1f, 30000.0f );
	perframe.view = LookAtMatrix( Vector3( 1.0, 0.4f, 1.0f ) * m_boundingSphere.radius, m_boundingSphere.center, Vector3( 0.0, 1.0, 0.0 ) );

	// Copy the current matrices to the current frame's uniform buffer
	// Note: Since we requested a host coherent memory type for the uniform buffer, the write is instantly visible to the GPU
	memcpy( m_perFrameBuffers[m_currentFrame].mapped, &perframe, sizeof( PerFrameData ) );

    
	vkCmdBindDescriptorSets( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shaderCache.GetPipelineLayout(), 0, 1, &m_perFrameBuffers[m_currentFrame].descriptorSet, 0, nullptr );
	vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh.pipeline );

	vkCmdBindVertexBuffers( commandBuffer, 0, 1, &vertexBuffer, offsets );

    if( lod.indexBuffer )
    {
		auto indexBuffer = lod.indexBuffer->GetGpuBuffer();

        vkCmdBindIndexBuffer( commandBuffer, indexBuffer, 0, lod.indexStride == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32 );
        vkCmdDrawIndexed( commandBuffer, lod.indexBuffer->size() / lod.indexStride, 1, 0, 0, 0 );
    }
    else
    {
        vkCmdDraw( commandBuffer, lod.vertexBuffer->size() / lod.vertexStride, 1, 0, 0 );
    }
    return VK_SUCCESS;
}

void ModelRenderer::SetData( const CmfContent* data, const Renderer* renderer )
{
	ReleaseMeshes( renderer );

	// Buffer copies have to be submitted to a queue, so we need a command buffer for them
	// Note: Some devices offer a dedicated transfer queue (with only the transfer bit set) that may be faster when doing lots of copies
	VkCommandBuffer copyCmd;
	auto logical = renderer->GetDevice()->GetLogicalDevice();
	VkCommandBufferAllocateInfo cmdBufAllocateInfo{};
	cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBufAllocateInfo.commandPool = renderer->GetCommandPool();
	cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdBufAllocateInfo.commandBufferCount = 1;
	ON_ERROR_LOG_AND_RETURN( vkAllocateCommandBuffers( logical, &cmdBufAllocateInfo, &copyCmd ), "Failed to allocate command buffer for model buffer creation" );
	
	VkCommandBufferBeginInfo cmdBufInfo{};
	cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	ON_ERROR_LOG_AND_RETURN( vkBeginCommandBuffer( copyCmd, &cmdBufInfo ), "Failed to begin command buffer for model buffer creation" );

	for( const auto& cmfMesh : data->m_cmfData->meshes )
    {
        Mesh mesh;
		std::vector<VkVertexInputAttributeDescription> vertexDescriptions;
		for( const auto& lod : cmfMesh.lods )
        {
            ModelLod modelLod;
            modelLod.vertexStride = lod.vb.stride;
            modelLod.indexStride = lod.ib.stride;

            auto vertexBufferOffset = data->m_cmfHeader->sections[lod.vb.index].offset + lod.vb.offset;

			modelLod.vertexBuffer = BufferBuilder::Build( renderer, data->m_fileContent.data() + vertexBufferOffset, lod.vb.size, BufferTypeVertex, lod.vb.stride );
			modelLod.vertexBuffer->CopyFromStaging( copyCmd );

            if( lod.ib.size > 0 )
            {
				auto indexBufferOffset = data->m_cmfHeader->sections[lod.ib.index].offset + lod.ib.offset;

				modelLod.indexBuffer = BufferBuilder::Build( renderer, data->m_fileContent.data() + indexBufferOffset, lod.ib.size, BufferTypeIndex, lod.ib.stride );
				modelLod.indexBuffer->CopyFromStaging( copyCmd );
            }

			if( vertexDescriptions.empty() )
			{
                uint32_t location = 0;
				for( const auto& decl : cmfMesh.decl )
				{
					VkVertexInputAttributeDescription attrDesc{};
					attrDesc.binding = 0;
					attrDesc.location = location++;
					attrDesc.offset = decl.offset;
					attrDesc.format = VulkanEnums::ElementTypeToVkFormat( decl.type, decl.elementCount );
					vertexDescriptions.push_back( attrDesc );
                }
            }
			mesh.stride = modelLod.vertexStride;
            mesh.lods.push_back( modelLod );
        }
        if( mesh.lods.size() > 0 )
        {
			mesh.vertexDescriptions = vertexDescriptions;
			m_meshes.push_back( mesh );
        }
	}

	ON_ERROR_LOG_AND_RETURN( vkEndCommandBuffer( copyCmd ), "Failed to end copy command" );
	
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
	ON_ERROR_LOG_AND_RETURN( vkCreateFence( logical, &fenceCI, nullptr, &fence ), "Failed to create fence for model buffer copy" );
	
	// Submit to the queue
	ON_ERROR_LOG_AND_RETURN( vkQueueSubmit( renderer->GetDevice()->GetGraphicsQueue(), 1, &submitInfo, fence ), "Failed to submit the copy queue" );
	// Wait for the fence to signal that command buffer has finished executing
	ON_ERROR_LOG_AND_RETURN( vkWaitForFences( logical, 1, &fence, VK_TRUE, 100000000000 ), "Failed to wait for fence" );
	
	vkDestroyFence( logical, fence, nullptr );
	vkFreeCommandBuffers( logical, renderer->GetCommandPool(), 1, &copyCmd );
	
	for( auto& mesh : m_meshes )
	{
		for( auto& lod : mesh.lods )
		{
			lod.vertexBuffer->ReleaseStaging( renderer );
			lod.indexBuffer->ReleaseStaging( renderer );
		}
	}

    SetShader( m_shaderName, renderer );
    m_boundingSphere = data->GetBoundingSphere();
}

void ModelRenderer::SetShader( std::string shaderName, const Renderer* renderer )
{
    m_shaderName = shaderName;

    auto logicalDevice = renderer->GetDevice()->GetLogicalDevice();
    for( auto& mesh : m_meshes )
    {
        if( mesh.pipeline != VK_NULL_HANDLE )
        {
			vkDestroyPipeline( logicalDevice, mesh.pipeline, renderer->GetAllocator() );
			mesh.pipeline = VK_NULL_HANDLE;
        }

        CR( m_shaderCache.CreatePipeline( renderer, shaderName, m_polygonMode, mesh.stride, mesh.vertexDescriptions, &mesh.pipeline ) );
    }
}

void ModelRenderer::SetPolygonMode( VkPolygonMode mode, const Renderer* renderer )
{
    m_polygonMode = mode;
	SetShader( m_shaderName, renderer );
}
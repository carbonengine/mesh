#include "model.h"

#include "../vulkan/vulkanerrors.h"

ModelRenderable::ModelRenderable( std::shared_ptr<const Renderer> renderer ) :
	m_renderer( renderer )
{
}

ModelRenderable::ModelRenderable( const CmfContent* data, std::shared_ptr<const Renderer> renderer ) :
	m_renderer( renderer )
{
	for( const auto& cmfMesh : data->m_cmfData->meshes )
	{
		m_meshes.push_back( { data, cmfMesh, renderer } );
	}
}

ModelRenderable::~ModelRenderable()
{
	m_meshes.clear();
}

VkResult ModelRenderable::Initialize()
{
	// Buffer copies have to be submitted to a queue, so we need a command buffer for them
	VkCommandBuffer copyCmd;
	auto logical = m_renderer->GetDevice()->GetLogicalDevice();
	VkCommandBufferAllocateInfo cmdBufAllocateInfo{};
	cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBufAllocateInfo.commandPool = m_renderer->GetCommandPool();
	cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdBufAllocateInfo.commandBufferCount = 1;
	RETURN_LOG_ERROR( vkAllocateCommandBuffers( logical, &cmdBufAllocateInfo, &copyCmd ), "Failed to allocate command buffer for model buffer creation" );

	VkCommandBufferBeginInfo cmdBufInfo{};
	cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	RETURN_LOG_ERROR( vkBeginCommandBuffer( copyCmd, &cmdBufInfo ), "Failed to begin command buffer for model buffer creation" );

	for( auto& mesh : m_meshes )
	{
		mesh.Initialize( copyCmd );
	}

	RETURN_LOG_ERROR( vkEndCommandBuffer( copyCmd ), "Failed to end copy command" );

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
	RETURN_LOG_ERROR( vkCreateFence( logical, &fenceCI, nullptr, &fence ), "Failed to create fence for model buffer copy" );

	// Submit to the queue
	RETURN_LOG_ERROR( vkQueueSubmit( m_renderer->GetDevice()->GetGraphicsQueue(), 1, &submitInfo, fence ), "Failed to submit the copy queue" );
	// Wait for the fence to signal that command buffer has finished executing
	RETURN_LOG_ERROR( vkWaitForFences( logical, 1, &fence, VK_TRUE, 100000000000 ), "Failed to wait for fence" );

	vkDestroyFence( logical, fence, nullptr );
	vkFreeCommandBuffers( logical, m_renderer->GetCommandPool(), 1, &copyCmd );

	for( auto& mesh : m_meshes )
	{
		mesh.Finalize();
	}
	return VK_SUCCESS;
}

void ModelRenderable::RenderMesh( CommandBuffer& commandBuffer, uint32_t lod, int32_t meshIndex, int32_t areaIndex )
{
	if( meshIndex == -1 )
	{
		for( const auto& mesh : m_meshes )
		{
			mesh.Render( commandBuffer, lod, areaIndex );
		}
	}
	else if( meshIndex >= 0 && static_cast<size_t>( meshIndex ) < m_meshes.size() )
	{
		m_meshes[meshIndex].Render( commandBuffer, lod, areaIndex );
	}
	else
	{
		Log::Error( "Selected mesh index out of bounds in Model::RenderMesh (%d requested)", meshIndex );
	}
}

VkResult ModelRenderable::SetRenderingMode( const ShaderCache* shaderCache, std::string shaderName, VkPolygonMode polygonMode )
{
	for( auto& mesh : m_meshes )
	{
		RETURN_ERROR( mesh.SetRenderingMode( shaderCache, shaderName, polygonMode ) );
	}
	return VK_SUCCESS;
}

void ModelRenderable::AddMeshRenderable( MeshRenderable&& meshRenderable )
{
	m_meshes.push_back( meshRenderable );
}
#include "primitive.h"

#include "../vulkan/vulkanerrors.h"

PrimitiveRenderable::PrimitiveRenderable( std::shared_ptr<const Renderer> renderer ) :
	m_renderer( renderer )
{
}

PrimitiveRenderable::~PrimitiveRenderable()
{
	vkDestroyPipeline( m_renderer->GetDevice()->GetLogicalDevice(), m_pipeline, m_renderer->GetAllocator() );
}


void PrimitiveRenderable::SetBufferData( const uint8_t* data, uint32_t size, uint32_t stride )
{
	m_stride = stride;
	m_size = size;
	m_data = data;
}

void PrimitiveRenderable::SetVertexDescriptions( const std::vector<VkVertexInputAttributeDescription>& vertexDescriptions )
{
	m_vertexDescriptions = vertexDescriptions;
}

void PrimitiveRenderable::SetLineWidth( float lineWidth )
{
	m_lineWidth = lineWidth;
}

void PrimitiveRenderable::SetTopology( VkPrimitiveTopology topology )
{
    m_topology = topology;
}

VkResult PrimitiveRenderable::Initialize()
{
	VkCommandBuffer copyCmd;

	RETURN_ERROR( m_renderer->CreateCopyCommandBuffer( &copyCmd ) );

	m_vertexBuffer = BufferBuilder::Build( m_renderer.get(), m_data, m_size, BufferType::Vertex, m_stride );
	m_vertexBuffer->CopyFromStaging( copyCmd );

	RETURN_ERROR( m_renderer->EndCopyCommandBuffer( copyCmd ) );

	m_vertexBuffer->ReleaseStaging( m_renderer.get() );
	return VK_SUCCESS;
}

void PrimitiveRenderable::Render( CommandBuffer& commandBuffer )
{
	commandBuffer.SetLineWidth( m_lineWidth );

	commandBuffer.BindPipeline( m_pipeline );
	commandBuffer.Render( m_vertexBuffer, nullptr, 0, m_size / m_stride );
}

VkResult PrimitiveRenderable::SetRenderingMode( const ShaderCache* shaderCache, std::string shaderName, VkPolygonMode polygonMode )
{
	auto logicalDevice = m_renderer->GetDevice()->GetLogicalDevice();
	CR_RETURN( vkDeviceWaitIdle( logicalDevice ) );
	if( m_pipeline != VK_NULL_HANDLE )
	{
		vkDestroyPipeline( logicalDevice, m_pipeline, m_renderer->GetAllocator() );
		m_pipeline = VK_NULL_HANDLE;
	}
	auto config = ShaderCache::PipelineConfig();
	config.topology = m_topology;
	config.polygonMode = polygonMode;
	config.lineWidth = m_lineWidth;
	CR_RETURN( shaderCache->CreatePipeline( shaderName, config, m_stride, m_vertexDescriptions, &m_pipeline ) );

	return VK_SUCCESS;
}

// Copyright © 2026 CCP ehf.

#include "primitive.h"

#include "../vulkan/vulkanerrors.h"

PrimitiveRenderable::PrimitiveRenderable( std::shared_ptr<const Renderer> renderer, GraphicsEffect&& effect ) :
	m_renderer( renderer ),
	m_effect( effect )
{
}

PrimitiveRenderable::~PrimitiveRenderable()
{
	if( m_vertexBuffer )
	{
		m_vertexBuffer->Release( m_renderer.get() );
		m_vertexBuffer = nullptr;
	}
	if( m_indexBuffer )
	{
		m_indexBuffer->Release( m_renderer.get() );
		m_indexBuffer = nullptr;
	}
	for( auto& storageBufferPair : m_storageBuffers )
	{
		storageBufferPair.second->Release( m_renderer.get() );
	}
}

GraphicsEffect& PrimitiveRenderable::GetEffect()
{
	return m_effect;
}

void PrimitiveRenderable::SetBufferData( const uint8_t* data, uint32_t size, uint32_t stride )
{
	assert( stride == m_effect.GetStride() );
	m_vertexStride = stride;
	m_vertexBufferSize = size;
	m_data = data;
}

void PrimitiveRenderable::SetIndexData( const uint8_t* indexData, uint32_t size, uint32_t stride )
{
	m_indexData = indexData;
	m_indexBufferSize = size;
	m_indexStride = stride;
}

VkResult PrimitiveRenderable::Initialize()
{
	VkCommandBuffer copyCmd;

	if( m_data != nullptr )
	{
		RETURN_ERROR( m_renderer->CreateCopyCommandBuffer( &copyCmd ) );

		m_vertexBuffer = BufferBuilder::Build( m_renderer.get(), m_data, m_vertexBufferSize, BufferType::Vertex, m_vertexStride );
		m_vertexBuffer->CopyFromStaging( copyCmd );
		m_elements = m_vertexBufferSize / m_vertexStride;
		if( m_indexData )
		{
			m_indexBuffer = BufferBuilder::Build( m_renderer.get(), m_indexData, m_indexBufferSize, BufferType::Index, m_indexStride );
			m_indexBuffer->CopyFromStaging( copyCmd );
			m_elements = m_indexBufferSize / m_indexStride;
		}

		RETURN_ERROR( m_renderer->EndCopyCommandBuffer( copyCmd ) );
		m_vertexBuffer->ReleaseStaging( m_renderer.get() );
		if( m_indexData )
		{
			m_indexBuffer->ReleaseStaging( m_renderer.get() );
		}
	}

	RETURN_ERROR( m_effect.Initialize() );
	return VK_SUCCESS;
}

void PrimitiveRenderable::Render( GraphicsCommandBuffer& commandBuffer, const Buffer* vertexBuffer, const Buffer* indexBuffer, uint32_t elements, uint32_t instanceCount )
{
	assert( vertexBuffer != nullptr );

	commandBuffer.BindEffect( m_effect );
	commandBuffer.BindVertexBuffer( vertexBuffer->GetGpuBuffer() );
	if( indexBuffer )
	{
		commandBuffer.BindIndexBuffer( *indexBuffer );
		commandBuffer.DrawIndexed( 0, elements, instanceCount );
	}
	else
	{
		commandBuffer.Draw( 0, elements, instanceCount );
	}
}

void PrimitiveRenderable::Render( GraphicsCommandBuffer& commandBuffer, uint32_t instanceCount )
{
	Render( commandBuffer, m_vertexBuffer, m_indexBuffer, m_elements, instanceCount );
}

#include "meshlod.h"

MeshLodRenderable::MeshLodRenderable( std::shared_ptr<const Renderer> renderer ) :
	m_renderer( renderer )
{
}

MeshLodRenderable::MeshLodRenderable( const CmfContent* data, cmf::MeshLod cmfLod, std::shared_ptr<const Renderer> renderer ) :
	m_renderer( renderer )
{
	auto vertexBufferOffset = data->m_cmfHeader->sections[cmfLod.vb.index].offset + cmfLod.vb.offset;

	m_vertexData = { data->m_fileContent.data() + vertexBufferOffset, cmfLod.vb.size, cmfLod.vb.stride };

	if( cmfLod.ib.size > 0 )
	{
		auto indexBufferOffset = data->m_cmfHeader->sections[cmfLod.ib.index].offset + cmfLod.ib.offset;
		m_indexData = { data->m_fileContent.data() + indexBufferOffset, cmfLod.ib.size, cmfLod.ib.stride };
	}


	for( const auto& area : cmfLod.areas )
	{
		// is this 3 correct? Where does it come from?
		m_areas.push_back( { area.firstElement * 3, area.elementCount * 3 } );
	}
}

MeshLodRenderable::~MeshLodRenderable()
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
}

void MeshLodRenderable::Initialize( VkCommandBuffer initializeCmd )
{
	m_vertexBuffer = BufferBuilder::Build( m_renderer.get(), m_vertexData.data, m_vertexData.size, BufferTypeVertex, m_vertexData.stride );
	m_vertexBuffer->CopyFromStaging( initializeCmd );

	if( m_indexData.data != nullptr )
	{
		m_indexBuffer = BufferBuilder::Build( m_renderer.get(), m_indexData.data, m_indexData.size, BufferTypeIndex, m_indexData.stride );
		m_indexBuffer->CopyFromStaging( initializeCmd );
	}
}

void MeshLodRenderable::Finalize()
{
	if( m_vertexBuffer )
	{
		m_vertexBuffer->ReleaseStaging( m_renderer.get() );
	}
	if( m_indexBuffer )
	{
		m_indexBuffer->ReleaseStaging( m_renderer.get() );
	}

	m_vertexData = {};
	m_indexData = {};
}

void MeshLodRenderable::Render( CommandBuffer& commandBuffer, int32_t areaIndex ) const
{
	if( m_vertexBuffer == nullptr )
	{
		Log::Error( "No vertex buffer set for mesh lod" );
		return;
	}

	if( !m_vertexBuffer->IsValid() )
	{
		Log::Error( "Vertex buffer is invalid for mesh lod" );
		return;
	}

	if( m_indexBuffer != nullptr && !m_indexBuffer->IsValid() )
	{
		Log::Error( "Index buffer is invalid for mesh lod" );
		return;
	}

	// Render each area separately. Perhaps this should be done in different colors?
	if( areaIndex == -1 )
	{
		for( uint32_t i = 0; i < m_areas.size(); i++ )
		{
			auto area = m_areas[i];
			commandBuffer.Render( m_vertexBuffer, m_indexBuffer, area.firstElement, area.elementCount );
		}
	}
	else if( areaIndex >= 0 && static_cast<size_t>( areaIndex ) < m_areas.size() )
	{
		auto area = m_areas[areaIndex];
		commandBuffer.Render( m_vertexBuffer, m_indexBuffer, area.firstElement, area.elementCount );
	}
	else
	{
		Log::Error( "Area index out of bounds in MeshLod::Render (%d requested)", areaIndex );
	}
}


void MeshLodRenderable::SetVertexData( const uint8_t* data, uint32_t size, uint32_t stride )
{
	m_vertexData = { data, size, stride };
}

void MeshLodRenderable::SetIndexData( const uint8_t* data, uint32_t size, uint32_t stride )
{
	m_indexData = { data, size, stride };
}

void MeshLodRenderable::AddArea( uint32_t firstElement, uint32_t elementCount )
{
	m_areas.push_back( { firstElement, elementCount } );
}
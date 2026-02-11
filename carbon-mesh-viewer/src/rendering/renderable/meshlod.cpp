#include "meshlod.h"

#include <cmf/declutils.h>

#include "../vulkan/vulkanenums.h"

MeshLodRenderable::MeshLodRenderable( CmfContent* data, const cmf::Mesh& cmfMesh, const cmf::MeshLod& cmfLod, std::shared_ptr<const Renderer> renderer ) :
	m_renderer( renderer ),
	m_data( data ),
	m_cmfMesh( cmfMesh ),
	m_cmfLod( cmfLod )
{
	m_vertexData = { data->Index( cmfLod.vb.index, cmfLod.vb.offset ), cmfLod.vb.size, cmfLod.vb.stride };

	if( cmfLod.ib.size > 0 )
	{
		m_indexData = { data->Index( cmfLod.ib.index, cmfLod.ib.offset ), cmfLod.ib.size, cmfLod.ib.stride };
	}

	VulkanEnums::AsVertexInputAttributeDescriptions( cmfMesh.morphTargets.decl, m_morphAttributeDescriptions );

	for( const auto& area : cmfLod.areas )
	{
		// is this 3 correct? Where does it come from?
		m_areas.push_back( { area.firstElement * 3, area.elementCount * 3 } );
	}

	for( uint32_t i = 0; i < 3; i++ )
	{
		m_modifiedVertexBuffer.push_back( nullptr );
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
	for( auto& vb : m_modifiedVertexBuffer )
	{
		if( vb )
		{
			vb->Release( m_renderer.get() );
		}
	}
}

void MeshLodRenderable::Initialize( VkCommandBuffer initializeCmd, size_t morphTargetStateIndex )
{
	m_morphTargetStateIndex = morphTargetStateIndex;
	m_vertexBuffer = BufferBuilder::Build( m_renderer.get(), m_vertexData.data, m_vertexData.size, BufferType::Vertex, m_vertexData.stride );
	m_vertexBuffer->CopyFromStaging( initializeCmd );

	if( m_indexData.data != nullptr )
	{
		m_indexBuffer = BufferBuilder::Build( m_renderer.get(), m_indexData.data, m_indexData.size, BufferType::Index, m_indexData.stride );
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
}

void MeshLodRenderable::Render( CommandBuffer& commandBuffer )
{
	if( m_currentVertexBuffer == nullptr )
	{
		Log::Error( "No vertex buffer set for mesh lod" );
		return;
	}

	if( !m_currentVertexBuffer->IsValid() )
	{
		Log::Error( "Vertex buffer is invalid for mesh lod" );
		return;
	}

	if( m_indexBuffer != nullptr && !m_indexBuffer->IsValid() )
	{
		Log::Error( "Index buffer is invalid for mesh lod" );
		return;
	}

	RenderBuffers( commandBuffer, m_currentVertexBuffer, m_indexBuffer );
}

void MeshLodRenderable::RenderBuffers( CommandBuffer& commandBuffer, Buffer* vb, Buffer* ib )
{
	// Render each area separately. Perhaps this should be done in different colors?
	for( uint32_t i = 0; i < m_areas.size(); i++ )
	{
		auto area = m_areas[i];
		commandBuffer.Render( vb, ib, area.firstElement, area.elementCount );
	}
}

void MeshLodRenderable::UpdateGeo( const AppState& appState )
{
	m_currentVertexBuffer = Morph( appState );
}

Buffer* MeshLodRenderable::Morph( const AppState& appState )
{
	if( !HasMorphs( appState ) )
	{
		return m_vertexBuffer;
	}

	BufferData morphedVertexData = {};
	morphedVertexData.data = (uint8_t*)malloc( m_vertexData.size );
	morphedVertexData.size = m_vertexData.size;
	morphedVertexData.stride = m_vertexData.stride;
	// copy the whole buffer, since the morphs may not modify all the data
	memcpy( (void*)morphedVertexData.data, (const void*)m_vertexData.data, m_vertexData.size );

	for( const auto& decl : m_cmfMesh.morphTargets.decl )
	{
		switch( decl.elementCount )
		{
		case 2:
			ApplyMorph<Vector2>( appState, *FindElement( m_cmfMesh.decl, decl.usage, decl.usageIndex ), morphedVertexData );
			break;
		case 3:
			ApplyMorph<Vector3>( appState, *FindElement( m_cmfMesh.decl, decl.usage, decl.usageIndex ), morphedVertexData );
			break;
		case 4:
			ApplyMorph<Vector4>( appState, *FindElement( m_cmfMesh.decl, decl.usage, decl.usageIndex ), morphedVertexData );
			break;
		default:
			Log::Error( "Unsupported morph target element count: %d", decl.elementCount );
			return m_vertexBuffer;
		}
	}

	auto currentFrame = m_renderer->GetCurrentFrame();
	if( m_modifiedVertexBuffer[currentFrame] )
	{
		m_modifiedVertexBuffer[currentFrame]->Release( m_renderer.get() );
		m_modifiedVertexBuffer[currentFrame] = nullptr;
	}

	m_modifiedVertexBuffer[currentFrame] = BufferBuilder::Build( m_renderer.get(), morphedVertexData.data, morphedVertexData.size, BufferType::Vertex, morphedVertexData.stride );

	VkCommandBuffer copyCmd = VK_NULL_HANDLE;
	m_renderer->CreateCopyCommandBuffer( &copyCmd );

	m_modifiedVertexBuffer[currentFrame]->CopyFromStaging( copyCmd );

	m_renderer->EndCopyCommandBuffer( copyCmd );

	m_modifiedVertexBuffer[currentFrame]->ReleaseStaging( m_renderer.get() );

	free( (void*)morphedVertexData.data );

	return m_modifiedVertexBuffer[currentFrame];
}

bool MeshLodRenderable::HasMorphs( const AppState& appState )
{
	for( size_t i = 0; i < m_cmfLod.morphTargets.size(); ++i )
	{
		bool enabled = appState.morphTargetEnabled[m_morphTargetStateIndex + i].GetValue();
		float weight = appState.morphTargetWeight[m_morphTargetStateIndex + i].GetValue();

		if( enabled && weight != 0.0f )
		{
			return true;
		}
	}
	return false;
}

template <typename T>
void MeshLodRenderable::ApplyMorph( const AppState& appState, const cmf::VertexElement& element, BufferData& outputBuffer )
{
	cmf::ConstBufferElementStream<T> baseData( element, m_vertexData.data, m_cmfLod.vb );
	std::vector<T> morphedData;

	morphedData.reserve( m_cmfLod.vb.size / m_cmfLod.vb.stride );
	uint32_t index = 0;
	for( const T data : baseData )
	{
		morphedData.push_back( data );
	}

	for( size_t i = 0; i < m_cmfLod.morphTargets.size(); ++i )
	{
		bool enabled = appState.morphTargetEnabled[m_morphTargetStateIndex + i].GetValue();
		float weight = appState.morphTargetWeight[m_morphTargetStateIndex + i].GetValue();

		if( !enabled || weight == 0.0f )
		{
			continue;
		}

		auto morphTarget = m_cmfLod.morphTargets[i];

		const uint8_t* data = m_data->Index( morphTarget.vb.index, morphTarget.vb.offset );
		auto morphElement = FindElement( m_cmfMesh.morphTargets.decl, element.usage, element.usageIndex );
		auto elementStream = cmf::ConstBufferElementStream<T>( *morphElement, data, morphTarget.vb );
		uint32_t index = 0;
		for( const T elementData : elementStream )
		{
			morphedData[index] += ( elementData - baseData[index] ) * weight;
			index++;
		}
	}

	static std::vector<cmf::Usage> normalizedElements{
		cmf::Usage::Tangent,
		cmf::Usage::Normal,
		cmf::Usage::Binormal,
		cmf::Usage::PackedTangent,
		cmf::Usage::PackedTangentLegacy
	};

	bool normalized = std::find( normalizedElements.begin(), normalizedElements.end(), element.usage ) != normalizedElements.end();

	for( uint32_t i = 0; i < morphedData.size(); i++ )
	{
		uint8_t* vertexDataPtr = (uint8_t*)outputBuffer.data + i * outputBuffer.stride + element.offset;
		T d = morphedData[i];
		if( normalized )
		{
			d /= Length( d );
		}
		memcpy( vertexDataPtr, &d, sizeof( T ) );
	}
}

template void MeshLodRenderable::ApplyMorph<Vector2>( const AppState& appState, const cmf::VertexElement& element, BufferData& outputBuffer );
template void MeshLodRenderable::ApplyMorph<Vector3>( const AppState& appState, const cmf::VertexElement& element, BufferData& outputBuffer );
template void MeshLodRenderable::ApplyMorph<Vector4>( const AppState& appState, const cmf::VertexElement& element, BufferData& outputBuffer );
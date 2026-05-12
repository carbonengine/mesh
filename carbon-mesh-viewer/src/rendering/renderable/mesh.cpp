#include "mesh.h"

#include "../models/boundingBox.h"
#include "../renderer.h"
#include "../vulkan/vulkanenums.h"
#include "../vulkan/vulkanerrors.h"

MeshRenderable::MeshRenderable( CmfContent* data, const cmf::Mesh& cmfMesh, std::shared_ptr<const Renderer> renderer ) :
	m_renderer( renderer ),
	m_cmfMesh( cmfMesh ),
	m_modelEffect( renderer ),
	m_wireframeEffect( renderer ),
	m_prepass( renderer, data, cmfMesh ),
	m_audioOcclusionRenderable( renderer, GetAudioOcclusionEffect( renderer, cmfMesh ) ),
	m_boundingBox( BoundingBox::Create( renderer, Vector3( 0.5, 0.5, 0.0 ) ) )
{
	for( const auto& vertexElement : m_cmfMesh.decl )
	{
		m_availableVertexElements.push_back( vertexElement );
	}
	m_boundingBoxTransform = ScalingMatrix( m_cmfMesh.bounds.Size() ) * TranslationMatrix( m_cmfMesh.bounds.Center() );
	m_stride = m_cmfMesh.lods[0].vb.stride;
}

void MeshRenderable::Initialize( AppState& appState )
{
	// Register mesh visibility state
	size_t stateIndex = appState.meshVisibilityStates.AddState();
	appState.meshVisibilityStates[stateIndex].RegisterCallback( [this]( bool visible, AppState& appState ) {
		m_display = visible;
	} );

	stateIndex = appState.meshWireframeOverlay.AddState();
	appState.meshWireframeOverlay[stateIndex].RegisterCallback( [this]( bool enabled, AppState& appState ) {
		m_wireframe = enabled;
	} );

	stateIndex = appState.audioOcclusionMesh.AddState();
	appState.audioOcclusionMesh[stateIndex].RegisterCallback( [this]( bool enabled, AppState& appState ) {
		m_audioOcclusion = enabled;
	} );

	stateIndex = appState.meshBoundingBox.AddState();
	appState.meshBoundingBox[stateIndex].RegisterCallback( [this]( bool enabled, AppState& appState ) {
		m_showBoundingBox = enabled;
	} );

	m_boundingBox.Initialize();

	appState.selectedLod.RegisterCallback( [this]( uint32_t lodIndex, AppState& appState ) {
		SetLod( lodIndex );
	} );

	SetLod( appState.selectedLod.GetValue() );

	m_prepass.Initialize( appState );
	if( !m_cmfMesh.audioOcclusionMesh.vertices.empty() && !m_cmfMesh.audioOcclusionMesh.indices.empty() )
	{
		m_audioOcclusionRenderable.SetBufferData(
			reinterpret_cast<const uint8_t*>( m_cmfMesh.audioOcclusionMesh.vertices.data() ),
			uint32_t( m_cmfMesh.audioOcclusionMesh.vertices.size() * sizeof( Vector3 ) ),
			sizeof( Vector3 ) );
		m_audioOcclusionRenderable.SetIndexData(
			reinterpret_cast<const uint8_t*>( m_cmfMesh.audioOcclusionMesh.indices.data() ),
			uint32_t( m_cmfMesh.audioOcclusionMesh.indices.size() * sizeof( uint16_t ) ),
			sizeof( uint16_t ) );
		m_audioOcclusionRenderable.Initialize();
	}
}

void MeshRenderable::SetLod( uint32_t lodLevel )
{
	if( lodLevel >= m_cmfMesh.lods.size() )
	{
		Log::Error( "Invalid LOD level %d for mesh %s. It only has %zu", lodLevel, ToStdString( m_cmfMesh.name ).c_str(), m_cmfMesh.lods.size() );
		return;
	}
	auto cmfLod = m_cmfMesh.lods[lodLevel];
	m_areas.clear();
	for( const auto& area : cmfLod.areas )
	{
		m_areas.push_back( { area.firstElement * 3, area.elementCount * 3 } );
	}
}

void MeshRenderable::Render( GraphicsCommandBuffer& commandBuffer, const AppState& appState, const Camera& camera )
{
	if( !m_display || !m_modelEffect.IsInitialized() )
	{
		return;
	}

	const Buffer indexBuffer = m_prepass.GetIndexBuffer();

	auto vertexBuffer = m_prepass.GetVertexBuffer();

	commandBuffer.BindVertexBuffer( vertexBuffer.GetGpuBuffer() );
	if( indexBuffer.IsValid() )
	{
		commandBuffer.BindIndexBuffer( m_prepass.GetIndexBuffer() );
	}

	auto viewProj = VertexUboData{ camera.GetProjection(), camera.GetView() };

	m_modelEffect.SetUniformData( 0, viewProj );

	commandBuffer.BindEffect( m_modelEffect );
	if( indexBuffer.IsValid() )
	{
		DrawIndexed( commandBuffer );
	}
	else
	{
		Draw( commandBuffer );
	}

	if( m_polygonMode != VK_POLYGON_MODE_LINE && m_wireframeEffect.IsInitialized() && m_wireframe )
	{
		m_wireframeEffect.SetUniformData( 0, viewProj );
		commandBuffer.BindVertexBuffer( vertexBuffer.GetGpuBuffer() );
		commandBuffer.BindEffect( m_wireframeEffect );
		if( indexBuffer.IsValid() )
		{
			commandBuffer.BindIndexBuffer( m_prepass.GetIndexBuffer() );
			DrawIndexed( commandBuffer );
		}
		else
		{
			Draw( commandBuffer );
		}
	}

	if( m_showBoundingBox )
	{
		auto vertexData = BoundingBox::VertexUBO{ camera.GetProjection(), camera.GetView(), m_boundingBoxTransform };
		m_boundingBox.SetUniformData( 0, vertexData );
		m_boundingBox.Render( commandBuffer );
	}

	if( m_audioOcclusion && !m_cmfMesh.audioOcclusionMesh.vertices.empty() && !m_cmfMesh.audioOcclusionMesh.indices.empty() )
	{
		m_audioOcclusionRenderable.SetUniformData( 0, viewProj );
		m_audioOcclusionRenderable.Render( commandBuffer );
	}
}

void MeshRenderable::PrepareMesh( ComputeCommandBuffer& commandBuffer )
{
	if( !m_display )
	{
		return;
	}
	m_prepass.Process( commandBuffer );
}

void MeshRenderable::Draw( GraphicsCommandBuffer& commandBuffer )
{
	for( uint32_t i = 0; i < m_areas.size(); i++ )
	{
		auto area = m_areas[i];
		commandBuffer.Draw( area.firstElement, area.elementCount );
	}
}

void MeshRenderable::DrawIndexed( GraphicsCommandBuffer& commandBuffer )
{
	for( uint32_t i = 0; i < m_areas.size(); i++ )
	{
		auto area = m_areas[i];
		commandBuffer.DrawIndexed( area.firstElement, area.elementCount );
	}
}

VkResult MeshRenderable::SetRenderingMode( std::string shaderName, VkPolygonMode polygonMode )
{
	auto logicalDevice = m_renderer->GetDevice()->GetLogicalDevice();

	m_polygonMode = polygonMode;
	m_shaderName = shaderName;

	CR_RETURN( vkDeviceWaitIdle( logicalDevice ) );

	auto config = GraphicsEffect::Config();
	config.topology = m_topology;
	config.polygonMode = polygonMode;
	config.cullMode = ( polygonMode == VK_POLYGON_MODE_FILL ) ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
	config.availableVertexElements = m_availableVertexElements;
	config.stride = m_stride;

	m_modelEffect.SetShaderName( m_shaderName );
	m_modelEffect.SetConfig( config );
	if( !m_modelEffect.IsInitialized() )
	{
		m_modelEffect.RegisterUniformData<VertexUboData>( VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, 0 );
		m_modelEffect.Initialize();
	}

	if( !m_wireframeEffect.IsInitialized() )
	{
		auto wireframeConfig = GraphicsEffect::Config();
		wireframeConfig.topology = m_topology;
		// use fill mode even though we are rendering wireframe
		// The reason is when we rasterize the lines we will get issues with the depth buffer where some lines
		// will fail the depth test and not get rendered.
		// We use barycentric coordinates in the shader to discard pixels that are not on the wireframe edges.
		wireframeConfig.polygonMode = VK_POLYGON_MODE_FILL;
		wireframeConfig.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
		wireframeConfig.cullMode = VK_CULL_MODE_NONE;
		wireframeConfig.blend = true;
		wireframeConfig.availableVertexElements = m_availableVertexElements;
		wireframeConfig.stride = m_stride;

		m_wireframeEffect.SetShaderName( "wireframeoverlay" );
		m_wireframeEffect.SetConfig( wireframeConfig );
		m_wireframeEffect.RegisterUniformData<VertexUboData>( VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, 0 );
		m_wireframeEffect.Initialize();
	}
	return VK_SUCCESS;
}

GraphicsEffect MeshRenderable::GetAudioOcclusionEffect( std::shared_ptr<const Renderer> renderer, const cmf::Mesh& cmfMesh )
{
	auto config = GraphicsEffect::Config();
	config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	config.polygonMode = VK_POLYGON_MODE_FILL;
	config.cullMode = VK_CULL_MODE_NONE;
	config.blend = false;
	config.stride = sizeof( Vector3 );
	config.availableVertexElements = {
		cmf::VertexElement{
			cmf::Usage::Position,
			0,
			cmf::ElementType::Float32,
			3,
			0 }
	};

	GraphicsEffect effect( renderer );
	effect.SetShaderName( "Face Normal" );
	effect.SetConfig( config );
	effect.RegisterUniformData<VertexUboData>( VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, 0 );

	return effect;
}

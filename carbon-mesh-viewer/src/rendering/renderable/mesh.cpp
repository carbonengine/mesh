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
	m_audioOcclusionRenderable( renderer, GetAudioOcclusionEffect( renderer ) ),
	m_boundingBox( BoundingBox::Create( renderer, Vector3( 0.5, 0.5, 0.0 ) ) )
{
	for( const auto& decl : cmfMesh.decl )
	{
		// Generate a predictable location so that shaders can find the attribute.
		uint32_t location = (uint32_t)decl.usage * 4u + decl.usageIndex;

		VkVertexInputAttributeDescription attrDesc{};
		attrDesc.binding = 0;
		attrDesc.location = location;
		attrDesc.offset = decl.offset;
		attrDesc.format = VulkanEnums::ElementTypeToVkFormat( decl.type, decl.elementCount );
		m_vertexDescriptions.push_back( attrDesc );
	}

	for( const auto& cmfLod : cmfMesh.lods )
	{
		m_lods.push_back( { data, cmfMesh, cmfLod, renderer } );
		m_stride = std::max( m_stride, cmfLod.vb.stride );
	}

	m_boundingBoxTransform = ScalingMatrix( m_cmfMesh.bounds.Size() ) * TranslationMatrix( m_cmfMesh.bounds.Center() );
}

MeshRenderable::~MeshRenderable()
{
}

void MeshRenderable::Initialize( AppState& appState, VkCommandBuffer initializeCmd )
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

	size_t morphTargetStateIndex = 0;
	// go through the morph targets so we can register to the morph weight and display flags
	for( size_t i = 0; i < m_cmfMesh.morphTargets.targets.size(); i++ )
	{
		auto index = appState.morphTargetEnabled.AddState();
		if( i == 0 )
		{
			morphTargetStateIndex = index;
		}
		appState.morphTargetWeight.AddState();
	}

	for( auto& lod : m_lods )
	{
		lod.Initialize( initializeCmd, morphTargetStateIndex );
	}
	m_boundingBox.Initialize();

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

void MeshRenderable::Finalize()
{
	for( auto& lod : m_lods )
	{
		lod.Finalize();
	}
}

void MeshRenderable::Render( CommandBuffer& commandBuffer, const AppState& appState, const Camera& camera, uint32_t lodIndex )
{
	if( !m_display )
	{
		return;
	}
	if( lodIndex >= m_lods.size() )
	{
		// maybe this is not an error, we should just skip rendering. But for now, log an error.
		Log::Error( "Lod index out of bounds in Mesh::Render (%d requested, %d available)", lodIndex, (uint32_t)m_lods.size() );
		return;
	}

	m_lods[lodIndex].UpdateGeo( appState );
	auto viewProj = VertexUboData{ camera.GetProjection(), camera.GetView() };

	m_modelEffect.SetUniformData( 0, viewProj );

	commandBuffer.BindEffect( m_modelEffect );
	m_lods[lodIndex].Render( commandBuffer );
	if( m_polygonMode != VK_POLYGON_MODE_LINE && m_wireframeEffect.IsInitialized() && m_wireframe )
	{
		m_wireframeEffect.SetUniformData( 0, viewProj );

		commandBuffer.BindEffect( m_wireframeEffect );
		m_lods[lodIndex].Render( commandBuffer );
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

VkResult MeshRenderable::SetRenderingMode( std::string shaderName, VkPolygonMode polygonMode )
{
	auto logicalDevice = m_renderer->GetDevice()->GetLogicalDevice();

	m_polygonMode = polygonMode;

	CR_RETURN( vkDeviceWaitIdle( logicalDevice ) );

	auto config = Effect::Config();
	config.topology = m_topology;
	config.polygonMode = polygonMode;
	config.cullMode = ( polygonMode == VK_POLYGON_MODE_FILL ) ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
	config.vertexStride = m_stride;
	config.vertexDescriptions = m_vertexDescriptions;
	m_modelEffect.SetShaderName( shaderName );
	m_modelEffect.SetConfig( config );
	if( !m_modelEffect.IsInitialized() )
	{
		m_modelEffect.RegisterUniformData<VertexUboData>( VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, 0 );
		m_modelEffect.Initialize();
	}

	if( !m_wireframeEffect.IsInitialized() )
	{
		auto wireframeConfig = Effect::Config();
		wireframeConfig.topology = m_topology;
		// use fill mode even though we are rendering wireframe
		// The reason is when we rasterize the lines we will get issues with the depth buffer where some lines
		// will fail the depth test and not get rendered.
		// We use barycentric coordinates in the shader to discard pixels that are not on the wireframe edges.
		wireframeConfig.polygonMode = VK_POLYGON_MODE_FILL;
		wireframeConfig.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
		wireframeConfig.cullMode = VK_CULL_MODE_NONE;
		wireframeConfig.blend = true;
		wireframeConfig.vertexStride = m_stride;
		wireframeConfig.vertexDescriptions = m_vertexDescriptions;
		m_wireframeEffect.SetShaderName( "wireframeoverlay" );
		m_wireframeEffect.SetConfig( wireframeConfig );
		m_wireframeEffect.RegisterUniformData<VertexUboData>( VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, 0 );
		m_wireframeEffect.Initialize();
	}
	return VK_SUCCESS;
}

Effect MeshRenderable::GetAudioOcclusionEffect( std::shared_ptr<const Renderer> renderer )
{
	auto config = Effect::Config();
	config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	config.polygonMode = VK_POLYGON_MODE_FILL;
	config.cullMode = VK_CULL_MODE_NONE;
	config.blend = false;
	config.vertexStride = sizeof( Vector3 );
	config.vertexDescriptions.push_back( { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 } );

	Effect effect( renderer );
	effect.SetShaderName( "facenormal" );
	effect.SetConfig( config );
	effect.RegisterUniformData<VertexUboData>( VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, 0 );

	return effect;
}

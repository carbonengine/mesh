#include "mesh.h"

#include "../renderer.h"
#include "../vulkan/vulkanenums.h"
#include "../vulkan/vulkanerrors.h"

MeshRenderable::MeshRenderable( CmfContent* data, const cmf::Mesh& cmfMesh, std::shared_ptr<const Renderer> renderer ) :
	m_renderer( renderer ),
	m_cmfMesh( cmfMesh )
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
}

MeshRenderable::~MeshRenderable()
{
	vkDestroyPipeline( m_renderer->GetDevice()->GetLogicalDevice(), m_pipeline, m_renderer->GetAllocator() );
	vkDestroyPipeline( m_renderer->GetDevice()->GetLogicalDevice(), m_wireframePipeline, m_renderer->GetAllocator() );
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
}

void MeshRenderable::Finalize()
{
	for( auto& lod : m_lods )
	{
		lod.Finalize();
	}
}

void MeshRenderable::Render( CommandBuffer& commandBuffer, const AppState& appState, uint32_t lodIndex )
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

	commandBuffer.BindPipeline( m_pipeline );
	m_lods[lodIndex].Render( commandBuffer );
	if( m_polygonMode != VK_POLYGON_MODE_LINE && m_wireframePipeline != VK_NULL_HANDLE && m_wireframe )
	{
		commandBuffer.BindPipeline( m_wireframePipeline );
		m_lods[lodIndex].Render( commandBuffer );
	}
}

VkResult MeshRenderable::SetRenderingMode( const ShaderCache* shaderCache, std::string shaderName, VkPolygonMode polygonMode )
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
	config.cullMode = ( polygonMode == VK_POLYGON_MODE_FILL ) ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
	CR_RETURN( shaderCache->CreatePipeline( shaderName, config, m_stride, m_vertexDescriptions, &m_pipeline ) );

	m_polygonMode = polygonMode;

	if( m_wireframePipeline == VK_NULL_HANDLE )
	{
		auto wireframeConfig = ShaderCache::PipelineConfig();
		wireframeConfig.topology = m_topology;
		// use fill mode even though we are rendering wireframe
		// The reason is when we rasterize the lines we will get issues with the depth buffer where some lines
		// will fail the depth test and not get rendered.
		// We use barycentric coordinates in the shader to discard pixels that are not on the wireframe edges.
		wireframeConfig.polygonMode = VK_POLYGON_MODE_FILL;
		wireframeConfig.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
		wireframeConfig.cullMode = VK_CULL_MODE_NONE;
		wireframeConfig.blend = true;

		CR_RETURN( shaderCache->CreatePipeline( "wireframeoverlay", wireframeConfig, m_stride, m_vertexDescriptions, &m_wireframePipeline ) );
	}
	return VK_SUCCESS;
}

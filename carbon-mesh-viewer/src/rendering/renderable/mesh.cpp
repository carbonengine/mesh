#include "mesh.h"

#include "../renderer.h"
#include "../vulkan/vulkanenums.h"
#include "../vulkan/vulkanerrors.h"

MeshRenderable::MeshRenderable( CmfContent* data, const cmf::Mesh& cmfMesh, std::shared_ptr<const Renderer> renderer ) :
	m_renderer( renderer ),
	m_data( data ),
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
}

void MeshRenderable::Initialize( AppState& appState, VkCommandBuffer initializeCmd )
{
	// Register mesh visibility state
	size_t meshIndex = appState.meshVisibilityStates.AddState();
	appState.meshVisibilityStates.RegisterCallback( meshIndex, [this]( bool visible, AppState& appState ) {
		m_display = visible;
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

	commandBuffer.BindPipeline( m_pipeline );

	m_lods[lodIndex].Render( commandBuffer, appState );
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
	CR_RETURN( shaderCache->CreatePipeline( shaderName, m_topology, polygonMode, 1.0f, m_stride, m_vertexDescriptions, &m_pipeline ) );

	return VK_SUCCESS;
}

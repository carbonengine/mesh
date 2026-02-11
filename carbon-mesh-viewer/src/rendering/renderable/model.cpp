#include "model.h"

#include "../vulkan/vulkanerrors.h"

ModelRenderable::ModelRenderable( CmfContent* data, std::shared_ptr<const Renderer> renderer ) :
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

VkResult ModelRenderable::Initialize( AppState& appState )
{
	// Buffer copies have to be submitted to a queue, so we need a command buffer for them
	VkCommandBuffer copyCmd;

    appState.meshVisibilityStates.Clear();
	appState.morphTargetEnabled.Clear();
	appState.morphTargetWeight.Clear();
	appState.meshWireframeOverlay.Clear();

	m_renderer->CreateCopyCommandBuffer( &copyCmd );
	for( auto& mesh : m_meshes )
	{
		mesh.Initialize( appState, copyCmd );
	}

	RETURN_ERROR( m_renderer->EndCopyCommandBuffer( copyCmd ) );

	for( auto& mesh : m_meshes )
	{
		mesh.Finalize();
	}
	return VK_SUCCESS;
}

void ModelRenderable::RenderMesh( CommandBuffer& commandBuffer, const AppState& state )
{
	uint32_t lod = state.selectedLod.GetValue();
	for( auto& mesh : m_meshes )
	{
		mesh.Render( commandBuffer, state, lod );
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

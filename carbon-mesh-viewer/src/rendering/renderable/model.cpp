#include "model.h"

#include "../models/boundingBox.h"
#include "../vulkan/vulkanerrors.h"

ModelRenderable::ModelRenderable( CmfContent* data, std::shared_ptr<const Renderer> renderer ) :
	m_renderer( renderer ),
	m_boundingBox( BoundingBox::Create( renderer, Vector3( 0.5, 0.5, 0.0 ) ) )
{
	CcpMath::AxisAlignedBox combined{};

	for( const auto& cmfMesh : data->m_cmfData->meshes )
	{
		m_meshes.push_back( { data, cmfMesh, renderer } );
		combined.IncludeBox( cmfMesh.bounds );
	}
	m_boundingBoxTransform = ScalingMatrix( combined.Size() ) * TranslationMatrix( combined.Center() );
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
	appState.meshBoundingBox.Clear();

	appState.modelBoundingBox.RegisterCallback( [&]( bool value, AppState& ) {
		m_showBoundingBox = value;
	} );

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

	m_boundingBox.Initialize();

	return VK_SUCCESS;
}

void ModelRenderable::RenderMesh( CommandBuffer& commandBuffer, const AppState& state, const Camera& camera )
{
	uint32_t lod = state.selectedLod.GetValue();
	for( auto& mesh : m_meshes )
	{
		mesh.Render( commandBuffer, state, camera, lod );
	}

	if( m_showBoundingBox )
	{
		auto vertexData = BoundingBox::VertexUBO{ camera.GetProjection(), camera.GetView(), m_boundingBoxTransform };
		m_boundingBox.SetUniformData( 0, vertexData );
		m_boundingBox.Render( commandBuffer );
	}
}

VkResult ModelRenderable::SetRenderingMode( std::string shaderName, VkPolygonMode polygonMode )
{
	for( auto& mesh : m_meshes )
	{
		RETURN_ERROR( mesh.SetRenderingMode( shaderName, polygonMode ) );
	}
	return VK_SUCCESS;
}

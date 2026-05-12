#include "model.h"

#include "../models/boundingBox.h"
#include "../vulkan/vulkanerrors.h"

ModelRenderable::ModelRenderable( std::shared_ptr<CmfContent> data, std::shared_ptr<const Renderer> renderer ) :
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
	appState.modelState.meshVisibilityStates.Clear();
	appState.modelState.morphTargetEnabled.Clear();
	appState.modelState.morphTargetWeight.Clear();
	appState.modelState.meshWireframeOverlay.Clear();
	appState.modelState.audioOcclusionMesh.Clear();
	appState.modelState.meshBoundingBox.Clear();

	appState.modelState.modelBoundingBox.RegisterCallback( [&]( bool value, AppState& ) {
		m_showBoundingBox = value;
	} );

	for( auto& mesh : m_meshes )
	{
		mesh.Initialize( appState );
	}

	m_boundingBox.Initialize();

	return VK_SUCCESS;
}

void ModelRenderable::RenderMesh( GraphicsCommandBuffer& commandBuffer, const AppState& state, const Camera& camera )
{
	for( auto& mesh : m_meshes )
	{
		mesh.Render( commandBuffer, state, camera );
	}

	if( m_showBoundingBox )
	{
		auto vertexData = BoundingBox::VertexUBO{ camera.GetProjection(), camera.GetView(), m_boundingBoxTransform };
		m_boundingBox.SetUniformData( 0, vertexData );
		m_boundingBox.Render( commandBuffer );
	}
}

VkResult ModelRenderable::PrepareModel( ComputeCommandBuffer& computeCommandBuffer )
{
	for( auto& mesh : m_meshes )
	{
		mesh.PrepareMesh( computeCommandBuffer );
	}
	return VK_SUCCESS;
}


VkResult ModelRenderable::SetRenderingMode( std::string shaderName, VkPolygonMode polygonMode )
{
	for( auto& mesh : m_meshes )
	{
		RETURN_ERROR( mesh.SetRenderingMode( shaderName, polygonMode ) );
	}
	return VK_SUCCESS;
}

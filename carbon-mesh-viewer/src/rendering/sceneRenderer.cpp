#include "sceneRenderer.h"

#include "vulkan/vulkanerrors.h"
#include "vulkan/vulkanenums.h"


SceneRenderer::SceneRenderer( std::shared_ptr<const Renderer> renderer, std::shared_ptr<ShaderCache> shaderCache ) :
	m_renderer( renderer ),
	m_shaderCache( shaderCache ),
	m_commandBuffer( renderer.get() )
{
}

SceneRenderer::~SceneRenderer()
{
	ReleaseModel();

	m_commandBuffer.Release( m_renderer.get() );
}

VkResult SceneRenderer::Initialize( AppState& state )
{
	state.cmfContent.RegisterCallback( [this]( CmfContent* content, const AppState& appstate ) {
		this->SetData( content, appstate );
	} );

	state.polygonMode.RegisterCallback( [this]( VkPolygonMode mode, const AppState& appstate ) {
		m_model->SetRenderingMode( m_shaderCache.get(), appstate.visualizationShader.GetValue(), mode );
	} );

	state.visualizationShader.RegisterCallback( [this]( std::string shaderName, const AppState& appstate ) {
		m_model->SetRenderingMode( m_shaderCache.get(), shaderName, appstate.polygonMode.GetValue() );
	} );

	state.windowSize.RegisterCallback( [this]( std::pair<uint32_t, uint32_t> size, const AppState& appstate ) {
		auto [width, height] = size;
		this->m_commandBuffer.SetRenderSize( width, height );
	} );

	CR_RETURN( m_commandBuffer.CreatePerFrameBuffers<PerFrameData>( m_renderer.get(), m_shaderCache.get() ) );

	auto [width, height] = state.windowSize.GetValue();
	m_commandBuffer.SetRenderSize( width, height );
	m_commandBuffer.SetRenderOffset( 0, 0 );
	m_commandBuffer.SetClearDepth( 1.0f );
	m_commandBuffer.SetClearColor( 0.1f, 0.1f, 0.1f );

	return VK_SUCCESS;
}

void SceneRenderer::ReleaseModel()
{
	auto logicalDevice = m_renderer->GetDevice()->GetLogicalDevice();
	vkDeviceWaitIdle( logicalDevice );

	if( m_model != nullptr )
	{
		m_model.release();
	}
}

VkResult SceneRenderer::Render( const AppState& state, const Camera& camera )
{
	CR_RETURN( m_commandBuffer.Begin( m_renderer.get() ) );

	// Update the perframe data
	PerFrameData perframe{};
	perframe.proj = camera.GetProjection();
	perframe.view = camera.GetView();

	m_commandBuffer.SetPerFrameData( perframe );

	if( m_model != nullptr )
	{
		m_model->RenderMesh( m_commandBuffer, state.selectedLod.GetValue(), state.selectedMesh.GetValue() );
	}

	CR_RETURN( m_commandBuffer.End() );

	return VK_SUCCESS;
}

void SceneRenderer::SetData( const CmfContent* data, const AppState& appstate )
{
	ReleaseModel();

	if( data == nullptr )
	{
		Log::Error( "No model data provided to SceneRenderer::SetData" );
		return;
	}

	m_model.reset( new ModelRenderable( data, m_renderer ) );
	m_model->Initialize();
	m_model->SetRenderingMode( m_shaderCache.get(), appstate.visualizationShader.GetValue(), appstate.polygonMode.GetValue() );
}

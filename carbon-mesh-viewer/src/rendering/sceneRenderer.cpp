#include "sceneRenderer.h"

#include "vulkan/shadercache.h"
#include "vulkan/vulkanerrors.h"
#include "vulkan/vulkanenums.h"

SceneRenderer::SceneRenderer( std::shared_ptr<Renderer> renderer ) :
	m_renderer( renderer ),
	m_graphicsCommandBuffer( renderer.get() )
{
}

SceneRenderer::~SceneRenderer()
{
	ReleaseModel();
}

VkResult SceneRenderer::Initialize( AppState& state )
{
	state.cmfContent.RegisterCallback( [this]( CmfContent* content, AppState& appState ) {
		this->SetData( content, appState );
	} );

	state.polygonMode.RegisterCallback( [this]( VkPolygonMode mode, AppState& appState ) {
		m_model->SetRenderingMode( appState.visualizationShader.GetValue(), mode );
	} );

	state.visualizationShader.RegisterCallback( [this]( std::string shaderName, AppState& appState ) {
		m_model->SetRenderingMode( shaderName, appState.polygonMode.GetValue() );
	} );

	state.windowSize.RegisterCallback( [this]( std::pair<uint32_t, uint32_t> size, AppState& appState ) {
		auto [width, height] = size;
		this->m_graphicsCommandBuffer.SetRenderSize( width, height );
	} );

	auto [width, height] = state.windowSize.GetValue();
	m_graphicsCommandBuffer.SetRenderSize( width, height );
	m_graphicsCommandBuffer.SetRenderOffset( 0, 0 );
	m_graphicsCommandBuffer.SetClearDepth( 1.0f );
	m_graphicsCommandBuffer.SetClearColor( 0.1f, 0.1f, 0.1f );

	return VK_SUCCESS;
}

void SceneRenderer::ReleaseModel()
{
	auto logicalDevice = m_renderer->GetDevice()->GetLogicalDevice();
	vkDeviceWaitIdle( logicalDevice );

	if( m_model != nullptr )
	{
		m_model.reset();
	}
}

void SceneRenderer::PrePass()
{
	if( m_model != nullptr )
	{
		m_computeCommandBuffer.Begin( m_renderer.get() );
		m_model->PrepareModel( m_computeCommandBuffer );
		m_computeCommandBuffer.End();
	}
}

void SceneRenderer::Render( const AppState& state, const Camera& camera )
{
	m_graphicsCommandBuffer.Begin( m_renderer.get() );

	if( m_model != nullptr )
	{
		m_model->RenderMesh( m_graphicsCommandBuffer, state, camera );
	}

	m_graphicsCommandBuffer.End();
}

void SceneRenderer::SetData( CmfContent* data, AppState& appState )
{
	ReleaseModel();

	if( data == nullptr )
	{
		Log::Error( "No model data provided to SceneRenderer::SetData" );
		return;
	}

	// reset the visualization shader if it is not set or not applicable for the current model
	std::string currentShaderName = appState.visualizationShader.GetValue();
	std::vector<cmf::VertexElement> availableVertexElements;

	for( const auto& mesh : data->m_cmfData->meshes )
	{
		// add all of the declarations, there may be situations where one mesh has more declarations than another inside the same model
		availableVertexElements.insert( availableVertexElements.end(), mesh.decl.begin(), mesh.decl.end() );
	}

	auto shaderNames = ShaderCache::GetAvailableShaderNames( availableVertexElements );
	auto foundItem = std::find_if( shaderNames.begin(), shaderNames.end(), [&]( auto name ) {
		return name == currentShaderName;
	} );

	if( foundItem == shaderNames.end() && shaderNames.size() > 0 )
	{
		appState.visualizationShader.SetValue( shaderNames[0] );
	}
	appState.availableShaders.SetValue( shaderNames );

	m_model.reset( new ModelRenderable( data, m_renderer ) );
	m_model->Initialize( appState );
	m_model->SetRenderingMode( appState.visualizationShader.GetValue(), appState.polygonMode.GetValue() );
}

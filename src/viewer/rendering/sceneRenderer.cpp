// Copyright © 2026 CCP ehf.

#include "sceneRenderer.h"

#include "vulkan/shadercache.h"
#include "vulkan/vulkanerrors.h"
#include "vulkan/vulkanenums.h"
#include "vulkan/graphicseffecttypes.h"

namespace
{
const std::vector<std::string> DEFAULT_SHADER_ORDER{
	"Normal",
	"Packed Normal",
	"Packed Normal Legacy",
	"Face Normal"
};
}

SceneRenderer::SceneRenderer( std::shared_ptr<Renderer> renderer ) :
	m_renderer( renderer ),
	m_graphicsCommandBuffer( renderer.get() ),
	m_depthLessDebugCommandBuffer( renderer.get() )
{
}

SceneRenderer::~SceneRenderer()
{
	ReleaseModel();
}

VkResult SceneRenderer::Initialize( AppState& state )
{
	state.cmfContent.RegisterCallback( [this]( std::shared_ptr<CmfContent> content, AppState& appState ) {
		this->SetData( content, appState );
	} );

	state.windowSize.RegisterCallback( [this]( std::pair<uint32_t, uint32_t> size, AppState& appState ) {
		auto [width, height] = size;
		this->m_graphicsCommandBuffer.SetRenderSize( width, height );
		this->m_depthLessDebugCommandBuffer.SetRenderSize( width, height );
	} );

	auto [width, height] = state.windowSize.GetValue();
	m_graphicsCommandBuffer.SetRenderSize( width, height );
	m_graphicsCommandBuffer.SetRenderOffset( 0, 0 );
	m_graphicsCommandBuffer.SetClearDepth( 1.0f );
	m_graphicsCommandBuffer.SetClearColor( 0.1f, 0.1f, 0.1f );

	// only clear depth so we can render bones inside of meshes etc.
	m_depthLessDebugCommandBuffer.SetRenderSize( width, height );
	m_depthLessDebugCommandBuffer.SetRenderOffset( 0, 0 );
	m_depthLessDebugCommandBuffer.SetClearDepth( 1.0f );

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

void SceneRenderer::Update( AppState& appState, const Camera& camera )
{
	if( m_model != nullptr )
	{
		m_model->Update( appState, camera );
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
		m_model->Render( m_graphicsCommandBuffer, state, camera );

		m_model->RenderDebug( m_graphicsCommandBuffer, state, camera );
	}

	m_graphicsCommandBuffer.End();

	m_depthLessDebugCommandBuffer.Begin( m_renderer.get() );
	if( m_model != nullptr )
	{
		m_model->RenderNoDepthDebug( m_depthLessDebugCommandBuffer, state, camera );
	}
	m_depthLessDebugCommandBuffer.End();
}

void SceneRenderer::SetData( std::shared_ptr<CmfContent> data, AppState& appState )
{
	ReleaseModel();
	appState.ResetModelState();

	if( data == nullptr )
	{
		Log::Error( "No model data provided to SceneRenderer::SetData" );
		return;
	}

	// reset the visualization shader if it is not set or not applicable for the current model
	auto [currentShaderName, currentShaderDeclaration] = appState.modelState.activeShader.GetValue();
	std::vector<cmf::VertexElement> availableVertexElements;

	for( const auto& mesh : data->m_cmfData->meshes )
	{
		// add all of the declarations, there may be situations where one mesh has more declarations than another inside the same model
		availableVertexElements.insert( availableVertexElements.end(), mesh.decl.begin(), mesh.decl.end() );
	}

	std::vector<std::pair<std::string, GraphicsEffectTypes::ShaderInputDeclaration>> shaders = ShaderCache::GetAvailableShaders( availableVertexElements );
	auto foundItem = std::find_if( shaders.begin(), shaders.end(), [name = currentShaderName]( const auto& shaderNameAndDeclarations ) {
		return shaderNameAndDeclarations.first == name;
	} );
	if( foundItem == shaders.end() && shaders.size() > 0 )
	{
		// pick the shader that best matches our default shader order
		auto defaultShaderIt = std::find_if( shaders.begin(), shaders.end(), [&]( auto shaderNameAndDeclarations ) {
			return std::find_if( DEFAULT_SHADER_ORDER.begin(), DEFAULT_SHADER_ORDER.end(), [&]( auto defaultShaderName ) {
					   return shaderNameAndDeclarations.first == defaultShaderName;
				   } ) != DEFAULT_SHADER_ORDER.end();
		} );

		if( defaultShaderIt != shaders.end() )
		{
			appState.modelState.activeShader.SetValue( *defaultShaderIt );
		}
		else
		{
			appState.modelState.activeShader.SetValue( shaders[0] );
		}
	}
	appState.modelState.availableShaders.SetValue( shaders );

	m_model.reset( new ModelRenderable( data, m_renderer ) );
	m_model->Initialize( appState );
}

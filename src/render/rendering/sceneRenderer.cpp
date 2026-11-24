// Copyright © 2026 CCP ehf.

#include "sceneRenderer.h"

#include <string_view>

#include "vulkan/shadercache.h"
#include "vulkan/vulkanerrors.h"
#include "vulkan/vulkanenums.h"
#include "vulkan/graphicseffecttypes.h"
#include "vulkan/rendertarget.h"

namespace
{
constexpr std::array<std::string_view, 4> DEFAULT_SHADER_ORDER{
	"Normal",
	"Packed Normal",
	"Packed Normal Legacy",
	"Face Normal"
};
constexpr float CLEAR_COLOR = 0.1f;
}

SceneRenderer::SceneRenderer( const std::shared_ptr<Renderer>& renderer ) :
	m_depthLessDebugCommandBuffer( renderer.get() ),
	m_graphicsCommandBuffer( renderer.get() ),
	m_renderer( renderer )
{
}

SceneRenderer::~SceneRenderer()
{
	ReleaseModel();
}

VkResult SceneRenderer::Initialize( uint32_t width, uint32_t height )
{
	m_graphicsCommandBuffer.SetRenderSize( width, height );
	m_graphicsCommandBuffer.SetRenderOffset( 0, 0 );
	m_graphicsCommandBuffer.SetClearDepth( 1.0f );
	m_graphicsCommandBuffer.SetClearColor( CLEAR_COLOR, CLEAR_COLOR, CLEAR_COLOR );

	// only clear depth so we can render bones inside of meshes etc.
	m_depthLessDebugCommandBuffer.SetRenderSize( width, height );
	m_depthLessDebugCommandBuffer.SetRenderOffset( 0, 0 );
	m_depthLessDebugCommandBuffer.SetClearDepth( 1.0f );

	return VK_SUCCESS;
}

void SceneRenderer::SetRenderSize( uint32_t width, uint32_t height )
{
	m_graphicsCommandBuffer.SetRenderSize( width, height );
	m_depthLessDebugCommandBuffer.SetRenderSize( width, height );
}

void SceneRenderer::SetRenderTarget( const RenderTarget* target )
{
	m_renderTarget = target;
	m_graphicsCommandBuffer.SetTarget( target );
	m_depthLessDebugCommandBuffer.SetTarget( target );
}

void SceneRenderer::SetClearColor( float r, float g, float b )
{
	m_graphicsCommandBuffer.SetClearColor( r, g, b );
}

void SceneRenderer::ReleaseModel()
{
	auto* logicalDevice = m_renderer->GetDevice()->GetLogicalDevice();
	vkDeviceWaitIdle( logicalDevice );

	if( m_model != nullptr )
	{
		m_model.reset();
	}
}

void SceneRenderer::Update( const Camera& camera )
{
	if( m_model != nullptr )
	{
		m_model->Update( camera );
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

void SceneRenderer::Render( const Camera& camera, const DebugDrawSettings& debugSettings )
{
	if( m_renderTarget != nullptr )
	{
		if( !m_renderTarget->IsValid() )
		{
			return;
		}
		m_renderTarget->BeginPass( m_renderer->GetCurrentGraphicVkCommandBuffer() );
	}

	m_graphicsCommandBuffer.Begin( m_renderer.get() );

	if( m_model != nullptr )
	{
		m_model->Render( m_graphicsCommandBuffer, camera );

		m_model->RenderDebug( m_graphicsCommandBuffer, camera );
	}

	m_graphicsCommandBuffer.End();

	m_depthLessDebugCommandBuffer.Begin( m_renderer.get() );
	if( m_model != nullptr )
	{
		m_model->RenderNoDepthDebug( m_depthLessDebugCommandBuffer, camera, debugSettings );
	}
	m_depthLessDebugCommandBuffer.End();

	if( m_renderTarget != nullptr )
	{
		m_renderTarget->EndPass( m_renderer->GetCurrentGraphicVkCommandBuffer() );
	}
}

SceneRenderer::ShaderList SceneRenderer::GetAvailableShaders( const CmfContent& data )
{
	std::vector<cmf::VertexElement> availableVertexElements;

	for( const auto& mesh : data.m_cmfData->meshes )
	{
		// add all of the declarations, there may be situations where one mesh has more declarations than another inside the same model
		availableVertexElements.insert( availableVertexElements.end(), mesh.decl.begin(), mesh.decl.end() );
	}

	return ShaderCache::GetAvailableShaders( availableVertexElements );
}

std::optional<SceneRenderer::ShaderEntry> SceneRenderer::ChooseShader( const ShaderList& shaders, const std::string& currentShaderName )
{
	auto foundItem = std::find_if( shaders.begin(), shaders.end(), [&currentShaderName]( const auto& shaderNameAndDeclarations ) {
		return shaderNameAndDeclarations.first == currentShaderName;
	} );
	if( foundItem != shaders.end() || shaders.empty() )
	{
		return std::nullopt;
	}
	// pick the shader that best matches our default shader order
	auto defaultShaderIt = std::find_if( shaders.begin(), shaders.end(), [&]( auto shaderNameAndDeclarations ) {
		return std::find_if( DEFAULT_SHADER_ORDER.begin(), DEFAULT_SHADER_ORDER.end(), [&]( const auto& defaultShaderName ) {
				   return shaderNameAndDeclarations.first == defaultShaderName;
			   } ) != DEFAULT_SHADER_ORDER.end();
	} );

	if( defaultShaderIt != shaders.end() )
	{
		return *defaultShaderIt;
	}
	return shaders[0];
}

ModelRenderable* SceneRenderer::SetData( const std::shared_ptr<CmfContent>& data )
{
	ReleaseModel();

	if( data == nullptr )
	{
		Log::Error( "No model data provided to SceneRenderer::SetData" );
		return nullptr;
	}

	m_model = std::make_unique<ModelRenderable>( data, m_renderer );
	m_model->Initialize();
	return m_model.get();
}

ModelRenderable* SceneRenderer::GetModel() const
{
	return m_model.get();
}

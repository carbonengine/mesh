#include "orientationGizmoRenderer.h"

#include "models/axis.h"
#include "renderable/mesh.h"
#include "renderable/meshlod.h"
#include "renderable/model.h"
#include "vulkan/vulkanerrors.h"
#include "vulkan/vulkanenums.h"

OrientationGizmoRenderer::OrientationGizmoRenderer( std::shared_ptr<const Renderer> renderer, std::shared_ptr<const ShaderCache> shaderCache ) :
	m_renderer( renderer ),
	m_shaderCache( shaderCache ),
	m_commandBuffer( renderer.get() ),
	m_axis( Axis::Create( renderer ) )
{
	m_commandBuffer.SetClearDepth( 1.0f );
	m_commandBuffer.CreatePerFrameBuffers<PerFrameData>( m_renderer.get(), m_shaderCache.get() );
}

OrientationGizmoRenderer::~OrientationGizmoRenderer()
{
	m_commandBuffer.Release( m_renderer.get() );
}

void OrientationGizmoRenderer::Initialize( AppState& state )
{
	m_axis.Initialize();
	m_axis.SetRenderingMode( m_shaderCache.get(), "orientationgizmo", VK_POLYGON_MODE_LINE );

	state.windowSize.RegisterCallback( [this]( std::pair<uint32_t, uint32_t> size, AppState& appState ) {
		auto [width, height] = size;
		SetSize( width, height );
	} );

	auto [width, height] = state.windowSize.GetValue();
	SetSize( width, height );
}

void OrientationGizmoRenderer::SetSize( uint32_t width, uint32_t height )
{
	auto minWidth = std::min( 100u, width );
	auto minHeight = std::min( 100u, height );
	auto gizmoSize = std::min( minWidth, minHeight );
	this->m_commandBuffer.SetRenderSize( gizmoSize, gizmoSize );
	this->m_commandBuffer.SetRenderOffset( width - gizmoSize - 10, height - gizmoSize - 10 );
	m_size = (float)gizmoSize;
}

VkResult OrientationGizmoRenderer::Render( const AppState& state, const Camera& camera )
{
	CR_RETURN( m_commandBuffer.Begin( m_renderer.get() ) );

	PerFrameData perFrameData{};
	perFrameData.proj = OrthoMatrix( 5.0f, 5.0f, 0.01f, 100.f );
	perFrameData.view = camera.GetRotation() * TranslationMatrix( 0.0f, 0.0f, -10.0f );

	m_commandBuffer.SetPerFrameData( perFrameData );
	m_axis.Render( m_commandBuffer );

	return m_commandBuffer.End();
}

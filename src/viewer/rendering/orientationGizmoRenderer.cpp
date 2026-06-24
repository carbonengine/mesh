// Copyright © 2026 CCP ehf.

#include "orientationGizmoRenderer.h"

#include "models/axis.h"
#include "models/primitiveEffects.h"

OrientationGizmoRenderer::OrientationGizmoRenderer( std::shared_ptr<const Renderer> renderer ) :
	m_renderer( renderer ),
	m_graphicsCommandBuffer( renderer.get() ),
	m_axis( Axis::CreateOrientationPrimitive( renderer ) )
{
	m_graphicsCommandBuffer.SetClearDepth( 1.0f );
}

void OrientationGizmoRenderer::Initialize( AppState& state )
{
	m_axis.Initialize();

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
	this->m_graphicsCommandBuffer.SetRenderSize( gizmoSize, gizmoSize );
	this->m_graphicsCommandBuffer.SetRenderOffset( width - gizmoSize - 10, height - gizmoSize - 10 );
	m_size = (float)gizmoSize;
}

void OrientationGizmoRenderer::Render( const AppState& state, const Camera& camera )
{
	m_graphicsCommandBuffer.Begin( m_renderer.get() );
	const PrimitiveEffects::VertexUBO ubo{
		OrthoMatrix( 5.0f, 5.0f, 0.01f, 100.f ),
		camera.GetRotation() * TranslationMatrix( 0.0f, 0.0f, -10.0f ),
		IdentityMatrix(),
		Vector4( 0.0f, 0.0f, 0.0f, 0.0f )
	};

	m_axis.SetUniformData( 0, ubo );
	m_axis.Render( m_graphicsCommandBuffer );

	m_graphicsCommandBuffer.End();
}

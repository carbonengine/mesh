// Copyright © 2026 CCP ehf.

#pragma once

#include "appState.h"
#include "camera.h"
#include "renderable/primitive.h"
#include "rendering/renderer.h"

// Handles showing an orientation gizmo
class OrientationGizmoRenderer
{
public:
	OrientationGizmoRenderer( std::shared_ptr<const Renderer> renderer );

	void Initialize( AppState& state );
	void Render( const AppState& state, const Camera& camera );

private:
	void SetSize( uint32_t width, uint32_t height );

	std::shared_ptr<const Renderer> m_renderer{ nullptr };

	PrimitiveRenderable m_axis;

	GraphicsCommandBuffer m_graphicsCommandBuffer;
	float m_size{ 0 };
};
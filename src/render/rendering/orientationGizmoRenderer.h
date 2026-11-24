// Copyright © 2026 CCP ehf.

#pragma once

#include "camera.h"
#include "renderable/primitive.h"
#include "rendering/renderer.h"

// Handles showing an orientation gizmo
class OrientationGizmoRenderer
{
public:
	OrientationGizmoRenderer( std::shared_ptr<const Renderer> renderer );

	void Initialize( uint32_t width, uint32_t height );
	void Render( const Camera& camera );
	/// Places the gizmo in the bottom right corner of a viewport of the given size
	void SetSize( uint32_t width, uint32_t height );

private:

	std::shared_ptr<const Renderer> m_renderer{ nullptr };

	PrimitiveRenderable m_axis;

	GraphicsCommandBuffer m_graphicsCommandBuffer;
	float m_size{ 0 };
};
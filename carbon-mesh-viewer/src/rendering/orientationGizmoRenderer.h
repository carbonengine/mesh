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
	~OrientationGizmoRenderer();

	void Initialize( AppState& state );
	VkResult Render( const AppState& state, const Camera& camera );

private:
	struct PerFrameData
	{
		Matrix proj;
		Matrix view;
	};

	void SetSize( uint32_t width, uint32_t height );

	std::shared_ptr<const Renderer> m_renderer{ nullptr };

	PrimitiveRenderable m_axis;

	CommandBuffer m_commandBuffer;
	float m_size{ 0 };
};
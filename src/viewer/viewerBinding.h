// Copyright © 2026 CCP ehf.

#pragma once

#include "appState.h"
#include "rendering/camera.h"
#include "rendering/orientationGizmoRenderer.h"
#include "rendering/sceneRenderer.h"

/// Connects the viewer AppState to the render library: state changes are forwarded to the renderer setters,
/// and renderer results (LODs, animated morph weights) are reflected back into the state for the UI.
class ViewerBinding
{
public:
	ViewerBinding( AppState& appState, SceneRenderer& sceneRenderer, Camera& camera, OrientationGizmoRenderer& gizmo );

	/// Registers the callbacks on the window, mouse and camera state
	void Initialize();
	/// Replaces the model shown by the scene renderer and rebuilds the model state
	void SetModel( const std::shared_ptr<CmfContent>& data );
	/// Pushes per-frame state into the renderer and reads back renderer results, call after SceneRenderer::Update
	void Sync();

	[[nodiscard]] DebugDrawSettings GetDebugDrawSettings() const;

private:
	void RegisterModelCallbacks();
	void AddMeshState( MeshRenderable& mesh );

	// not owned, the application outlives the binding
	AppState* m_appState;
	SceneRenderer* m_sceneRenderer;
	Camera* m_camera;
	OrientationGizmoRenderer* m_gizmo;
};

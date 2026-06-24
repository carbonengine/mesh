// Copyright © 2025 CCP ehf.

#pragma once

#include "appState.h"
#include "rendering/camera.h"
#include "rendering/orientationGizmoRenderer.h"
#include "rendering/sceneRenderer.h"
#include "rendering/renderer.h"
#include "rendering/uiRenderer.h"

class Application
{
public:
	Application();
	~Application();

	void Initialize();
	void Run();
	void LoadCmfFile( const std::string& path );

private:
	void OnMouseButton( int button, int action, int mods );
	void OnMouseMove( double xpos, double ypos );
	void OnKey( int key, int scancode, int action, int mods );
	void OnMouseScroll( double xoffset, double yoffset );

	void Resize( uint32_t width, uint32_t height );

	std::unique_ptr<SceneRenderer> m_sceneRenderer{ nullptr };
	std::shared_ptr<Renderer> m_renderer{ nullptr };
	std::unique_ptr<UIRenderer> m_uiRenderer{ nullptr };
	std::unique_ptr<OrientationGizmoRenderer> m_orientationGizmoRenderer{ nullptr };

	GLFWwindow* m_window{ nullptr };

	bool m_minimized{ false };

	AppState m_appState{};
	Camera m_camera{};

	bool m_valid{ false };
};

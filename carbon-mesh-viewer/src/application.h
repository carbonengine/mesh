#pragma once

#include "appState.h"
#include "rendering/modelRenderer.h"
#include "rendering/renderer.h"
#include "rendering/uiRenderer.h"

class Application
{
public:
	Application();
	~Application();

	void Initialize();
	void Run();
	void SetData( CmfContent* data );

private:
	void OnMouseButton( int button, int action, int mods );
	void OnMouseMove( double xpos, double ypos );
	void OnKey( int key, int scancode, int action, int mods );
	void OnMouseScroll( double xoffset, double yoffset );

	void Resize( uint32_t width, uint32_t height );

	std::unique_ptr<ModelRenderer> m_modelRenderer{ nullptr };
	std::shared_ptr<Renderer> m_renderer{ nullptr };
	std::unique_ptr<UIRenderer> m_uiRenderer{ nullptr };
	GLFWwindow* m_window{ nullptr };

	AppState m_appState{};
	Camera m_camera{};
};

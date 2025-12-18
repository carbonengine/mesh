#pragma once

#include "data/cmfcontent.h"
#include "rendering/renderer.h"
#include "rendering/modelRenderer.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "input/mousestate.h"

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

	ModelRenderer* m_modelRenderer{ nullptr };
	Renderer* m_renderer{ nullptr };

	GLFWwindow* m_window{ nullptr };

	CmfContent* m_cmfContent{ nullptr };

	MouseState m_mouseState = MouseState();
};
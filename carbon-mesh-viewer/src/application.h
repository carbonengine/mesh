#pragma once

#include "data/cmfdata.h"
#include "rendering/renderer.h"
#include "rendering/modelRenderer.h"


#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class Application
{
public:
	Application();
	~Application();

	void Initialize();
	void Run();
	void SetData( CmfData* data );

private:
    void OnMouseButton( int button, int action, int mods );
	void OnMouseMove( double xpos, double ypos );
	void OnKey( int key, int scancode, int action, int mods );

    void Resize( int width, int height );

    ModelRenderer* m_modelRenderer { nullptr };
	Renderer* m_renderer { nullptr };

    GLFWwindow* m_window { nullptr };

    CmfData* m_cmfData { nullptr };
};
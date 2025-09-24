#include "app.h"
#include <CCPLog.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

static void error_callback( int error, const char* description )
{
	fprintf( stderr, "Error: %s\n", description );
}

CarbonMeshViewerApp::CarbonMeshViewerApp()
{
    glfwSetErrorCallback( error_callback );
	if( !glfwInit() )
	{
        return;
	}

    if( !glfwVulkanSupported() )
    {
        printf( "GLFW: Vulkan Not Supported \n" );
        return;
	}

	glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 3 );
	glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 3 );
	glfwWindowHint( GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE );

	uint32_t count;
	const char** extensions = glfwGetRequiredInstanceExtensions( &count );

	m_window = glfwCreateWindow( 1280, 1024, "Carbon Mesh Viewer", NULL, NULL );
	if( !m_window )
	{
		CCP_LOGERR( "Failed to create window" );
        return;
	}

    this->m_renderContext = new RenderContext();
	if( !this->m_renderContext->Init( m_window ) )
	{
	    return;
    }
}

CarbonMeshViewerApp::~CarbonMeshViewerApp() {

    if( this->m_window )
    {
	    glfwDestroyWindow( this->m_window );
	    this->m_window = nullptr;
    }
	glfwTerminate();
}

void CarbonMeshViewerApp::run() {
	if( !this->m_window )
    {
        return;
    }
	while( !glfwWindowShouldClose( this->m_window ) )
	{
        // handle input
        

        // update
        m_renderContext->Update( 0.0f );

        // render
        m_renderContext->Render();

		// Keep running
		glfwPollEvents();
	}
}


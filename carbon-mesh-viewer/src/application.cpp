#include "application.h"

#include "model/Model.h"

Application::Application() :
	m_window( nullptr ),
	m_inputHandler( nullptr ),
	m_renderer( nullptr )
{
}

Application::~Application()
{
	if( m_window )
	{
		glfwDestroyWindow( m_window );
	}
	glfwTerminate();
}


void Application::init()
{
	// Initialize GLFW
	if( !glfwInit() )
	{
		CCP_LOGERR( "Failed to initialize GLFW" );
		return;
	}
	glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );

	// Create a windowed mode window and its OpenGL context
	m_window = glfwCreateWindow( 2048, 1024, "Carbon Mesh Viewer", nullptr, nullptr );
	if( !m_window )
	{
		CCP_LOGERR( "Failed to create GLFW window" );
		return;
	}

	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions( &glfwExtensionCount );

	std::vector<const char*> extensions( glfwExtensions, glfwExtensions + glfwExtensionCount );

	uint32_t width, height;
	int w, h;
	glfwGetWindowSize( m_window, &w, &h );
	width = static_cast<uint32_t>( w );
	height = static_cast<uint32_t>( h );

	// Initialize renderer
	m_renderer = new Renderer();
	if( m_renderer->CreateInstance( extensions ) != VK_SUCCESS )
	{
		CCP_LOGERR( "Failed to create Vulkan instance" );
		return;
	}
	VkSurfaceKHR windowSurface;
	if( glfwCreateWindowSurface( m_renderer->GetVulkanInstance(), m_window, nullptr, &windowSurface ) != VK_SUCCESS )
	{
		CCP_LOGERR( "Failed to create window surface" );
		return;
	}
	m_renderer->init( width, height, windowSurface );

	glfwSetWindowUserPointer( m_window, this );

	// initialize input handler
	m_inputHandler = new InputHandler();
	glfwSetKeyCallback( m_window, []( GLFWwindow* window, int key, int scancode, int action, int mods ) {
		Application* app = reinterpret_cast<Application*>( glfwGetWindowUserPointer( window ) );
		if( app && app->m_inputHandler )
		{
			app->m_inputHandler->KeyCallback( key, scancode, action, mods );
		}
	} );

	glfwSetMouseButtonCallback( m_window, []( GLFWwindow* window, int button, int action, int mods ) {
		Application* app = reinterpret_cast<Application*>( glfwGetWindowUserPointer( window ) );
		if( app && app->m_inputHandler )
		{
			if( button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS )
			{
				app->callUpdate = true;
			}
			else
			{
				app->callUpdate = false;
			}
			app->m_inputHandler->MouseButtonCallback( button, action, mods );
		}
	} );

	glfwSetCursorPosCallback( m_window, []( GLFWwindow* window, double x, double y ) {
		Application* app = reinterpret_cast<Application*>( glfwGetWindowUserPointer( window ) );
		if( app && app->m_inputHandler )
		{
			app->m_inputHandler->MouseCallback( x, y );
		}
	} );

	glfwSetDropCallback( m_window, []( GLFWwindow* window, int count, const char** paths ) {
		Application* app = reinterpret_cast<Application*>( glfwGetWindowUserPointer( window ) );
		if( app && app->m_inputHandler )
		{
			app->m_renderer->SetModel( ModelLoader::LoadModelFromFile( paths[0] ) );
		}
	} );
}

void Application::run()
{
	if( !m_window || !m_renderer )
	{
		return;
	}
	float time = (float)glfwGetTime();
	while( !glfwWindowShouldClose( m_window ) )
	{
		// Poll for and process events
		glfwPollEvents();
		float newTime = (float)glfwGetTime();
		if( callUpdate )
		{
			m_renderer->Update( ( newTime - time ) * 0.001f );
		}
		m_renderer->RenderFrame();
		time = newTime;
	}
}

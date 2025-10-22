#include "application.h"
#include <algorithm>

Application::Application() :
	m_modelRenderer( nullptr ),
	m_renderer( nullptr )
{
}

Application::~Application()
{
}

void Application::Initialize()
{
	// Initialize GLFW
	if( !glfwInit() )
	{
		CCP_LOGERR( "Failed to initialize GLFW" );
		return;
	}

	uint32_t width = 2048;
	uint32_t height = 1024;
	glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );

	// Create a windowed mode window and its OpenGL context
	m_window = glfwCreateWindow( width, height, "Carbon Mesh Viewer", nullptr, nullptr );
	if( !m_window )
	{
		CCP_LOGERR( "Failed to create GLFW window" );
		return;
	}

	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions( &glfwExtensionCount );

	std::vector<const char*> extensions( glfwExtensions, glfwExtensions + glfwExtensionCount );

	// Initialize renderer
	m_renderer = new Renderer();
	m_renderer->Resize( width, height );

	if( m_renderer->CreateInstance( extensions ) )
	{
		CCP_LOGERR( "Failed to create Vulkan instance" );
		return;
	}

	if( glfwCreateWindowSurface( m_renderer->GetVulkanInstance(), m_window, nullptr, m_renderer->GetSurface() ) != VK_SUCCESS )
	{
		CCP_LOGERR( "Failed to create window surface" );
		return;
	}

	m_renderer->Initialize();

	if( !m_renderer->IsValid() )
	{
		m_renderer = nullptr;
		CCP_LOGERR( "Failed to initialize renderer" );
		return;
	}

	glfwSetWindowUserPointer( m_window, this );

	m_modelRenderer = new ModelRenderer();
	m_modelRenderer->Initialize( m_renderer );
	m_modelRenderer->SetShader( "test", m_renderer );

	// initialize input handler
	glfwSetKeyCallback( m_window, []( GLFWwindow* window, int key, int scancode, int action, int mods ) {
		Application* app = reinterpret_cast<Application*>( glfwGetWindowUserPointer( window ) );
		if( app )
		{
			app->OnKey( key, scancode, action, mods );
		}
	} );

	glfwSetMouseButtonCallback( m_window, []( GLFWwindow* window, int button, int action, int mods ) {
		Application* app = reinterpret_cast<Application*>( glfwGetWindowUserPointer( window ) );
		if( app )
		{
			app->OnMouseButton( button, action, mods );
		}
	} );

	glfwSetCursorPosCallback( m_window, []( GLFWwindow* window, double x, double y ) {
		Application* app = reinterpret_cast<Application*>( glfwGetWindowUserPointer( window ) );
		if( app )
		{
			app->OnMouseMove( x, y );
		}
	} );

    glfwSetScrollCallback(m_window, []( GLFWwindow* window, double xoffset, double yoffset ){
		Application* app = reinterpret_cast<Application*>( glfwGetWindowUserPointer( window ) );
		if( app )
		{
			app->OnMouseScroll( xoffset, yoffset );
		}
    } );


	glfwSetDropCallback( m_window, []( GLFWwindow* window, int count, const char** paths ) {
		Application* app = reinterpret_cast<Application*>( glfwGetWindowUserPointer( window ) );
		if( app )
		{
			app->SetData( CmfContentLoader::LoadContentFromFile( paths[0] ) );
		}
	} );

	glfwSetWindowSizeCallback( m_window, []( GLFWwindow* window, int width, int height ) {
		Application* app = reinterpret_cast<Application*>( glfwGetWindowUserPointer( window ) );
		if( app )
		{
			app->Resize( width, height );
		}
	} );

	glfwSetWindowMaximizeCallback( m_window, []( GLFWwindow* window, int maximized ) {
		Application* app = reinterpret_cast<Application*>( glfwGetWindowUserPointer( window ) );
		if( app )
		{
			int width, height;
			glfwGetWindowSize( window, &width, &height );
			app->Resize( width, height );
		}
	} );
}

void Application::Run()
{
	if( !m_renderer )
	{
		return;
	}
	float time = (float)glfwGetTime();
	m_mouseState.UpdateScreenSize( m_renderer->GetWidth(), m_renderer->GetHeight() );

	while( !glfwWindowShouldClose( m_window ) )
	{
		// Poll for and process events
		glfwPollEvents();
		float newTime = (float)glfwGetTime();

		if( m_renderer->BeginRender() != VK_SUCCESS )
		{
			CCP_LOGERR( "Failed to begin render" );
			break;
		}
		if( m_cmfContent )
		{
			m_modelRenderer->Update( newTime - time, m_mouseState );
			m_modelRenderer->SetPerFrameData( m_renderer );

			for( size_t meshIndex = 0; meshIndex < m_cmfContent->m_cmfData->meshes.size(); meshIndex++ )
			{
				m_modelRenderer->RenderMesh( m_renderer, meshIndex, 0 );
			}
		}

		if( m_renderer->EndRender() != VK_SUCCESS )
		{
			CCP_LOGERR( "Failed to end  nder" );
			break;
		}
		time = newTime;
        m_mouseState.Clean();
	}

	if( m_window )
	{
		glfwDestroyWindow( m_window );
	}
	glfwTerminate();
}

void Application::SetData( CmfContent* data )
{
	if( !data )
	{
		CCP_LOGERR( "Invalid CMF data. Ignoring" );
		return;
	}
	m_cmfContent = data;
	m_modelRenderer->SetData( m_cmfContent, m_renderer );
}

void Application::OnMouseButton( int button, int action, int mods )
{
	if( action == 1 )
	{
		m_mouseState.PressButton( (MouseButton)button );
    }
	else if( action == 0 )
	{
		m_mouseState.ReleaseButton( (MouseButton)button );
    }
}

void Application::OnMouseScroll( double xoffset, double yoffset )
{
	m_mouseState.UpdateScroll( (float)xoffset, (float)yoffset );
}

void Application::OnMouseMove( double xpos, double ypos )
{
	m_mouseState.UpdatePosition( (float)xpos, (float)ypos );
}

void Application::OnKey( int key, int scancode, int action, int mods )
{
}

void Application::Resize( uint32_t width, uint32_t height )
{
	if( width == 0 || height == 0 )
	{
		return;
	}

	if( m_renderer )
	{
		if( width != m_renderer->GetWidth() || height != m_renderer->GetHeight() )
		{
			m_renderer->PreResize();
			m_renderer->ReleaseSurface();
			glfwCreateWindowSurface( m_renderer->GetVulkanInstance(), m_window, nullptr, m_renderer->GetSurface() );
			m_mouseState.UpdateScreenSize( width, height );
			m_renderer->Resize( width, height );
			m_modelRenderer->Resize( width, height );
		}
	}
}
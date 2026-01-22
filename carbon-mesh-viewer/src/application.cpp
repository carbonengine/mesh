#include "application.h"

#include <algorithm>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

Application::Application()
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
		Log::Error( "Failed to initialize GLFW" );
		return;
	}
	if( !glfwVulkanSupported() )
	{
		Log::Error( "GLFW was not built with Vulkan support or no Vulkan loader was found!" );
		return;
	}
	uint32_t desiredWidth = 2048;
	uint32_t desiredHeight = 1024;
	glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );

	// Create a windowed mode window and its OpenGL context
	m_window = glfwCreateWindow( desiredWidth, desiredHeight, "Carbon Mesh Viewer", nullptr, nullptr );
	if( !m_window )
	{
		Log::Error( "Failed to create GLFW window" );
		return;
	}


	// we may ask for a given size (see above) but it is not guaranteed to be the actual framebuffer size
	int actualHeight, actualWidth;
	glfwGetFramebufferSize( m_window, &actualWidth, &actualHeight );
	uint32_t width = static_cast<uint32_t>( actualWidth );
	uint32_t height = static_cast<uint32_t>( actualHeight );

	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions( &glfwExtensionCount );
	if( !glfwExtensions )
	{
		const char* errorDesc = nullptr;
		int errorCode = glfwGetError( &errorDesc );
		Log::Error( "glfwGetRequiredInstanceExtensions returned nullptr. GLFW error %d: %s", errorCode, errorDesc ? errorDesc : "(no description)" );
	}

	std::vector<const char*> extensions( glfwExtensions, glfwExtensions + glfwExtensionCount );
#ifdef APPLE
	extensions.push_back( VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME );
#endif
	// Initialize renderer
	m_renderer = std::make_shared<Renderer>( m_appState );
	m_renderer->Resize( width, height );

	if( m_renderer->CreateInstance( extensions ) )
	{
		Log::Error( "Failed to create Vulkan instance" );
		return;
	}

	if( glfwCreateWindowSurface( m_renderer->GetVulkanInstance(), m_window, nullptr, m_renderer->GetSurface() ) != VK_SUCCESS )
	{
		Log::Error( "Failed to create window surface" );
		return;
	}

	m_renderer->Initialize();

	if( !m_renderer->IsValid() )
	{
		m_renderer = nullptr;
		Log::Error( "Failed to initialize renderer" );
		return;
	}

	glfwSetWindowUserPointer( m_window, this );

	m_modelRenderer = std::make_unique<ModelRenderer>( m_renderer );
	m_modelRenderer->Initialize( m_appState );

	m_modelRenderer->SetShader( m_appState.visualizationShader.GetValue() );
	m_modelRenderer->SetPolygonMode( m_appState.polygonMode.GetValue() );

	m_uiRenderer = std::make_unique<UIRenderer>( m_renderer );
	m_uiRenderer->Initialize( m_window, m_appState );

	m_camera.Initialize( m_appState );

	m_appState.windowSize.SetValue( std::make_pair( width, height ) );

	// initialize input handler
	glfwSetDropCallback( m_window, []( GLFWwindow* window, int count, const char** paths ) {
		Application* app = reinterpret_cast<Application*>( glfwGetWindowUserPointer( window ) );
		if( app )
		{
			app->m_appState.cmfPath.SetValue( std::string( paths[0] ) );
			app->m_appState.cmfContent.SetValue( CmfContentLoader::LoadContentFromFile( paths[0] ) );
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
			int actualHeight, actualWidth;
			glfwGetFramebufferSize( window, &actualWidth, &actualHeight );
			app->Resize( actualWidth, actualHeight );
		}
	} );

	// add a state callback for window change so it happens at the right time
	m_appState.windowSize.RegisterCallback( [this]( std::pair<uint32_t, uint32_t> size ) {
		m_renderer->PreResize();
		m_renderer->ReleaseSurface();
		glfwCreateWindowSurface( m_renderer->GetVulkanInstance(), m_window, nullptr, m_renderer->GetSurface() );
		this->m_renderer->Resize( size.first, size.second );
	} );
}

void Application::Run()
{
	if( !m_renderer )
	{
		return;
	}
	float time = (float)glfwGetTime();

	while( !glfwWindowShouldClose( m_window ) )
	{
		m_appState.CallStateCallbacks();

		// Poll for and process events
		glfwPollEvents();

		float newTime = (float)glfwGetTime();

		if( m_renderer->BeginRender() != VK_SUCCESS )
		{
			Log::Error( "Failed to begin render" );
			break;
		}
		m_uiRenderer->BeginFrame();

		m_camera.Update( newTime - time );

		if( m_appState.cmfContent.GetValue() )
		{
			m_modelRenderer->SetPerFrameData();
			m_modelRenderer->RenderMesh( m_appState, m_camera );
		}

		m_uiRenderer->UpdateInputs( m_appState );
		m_uiRenderer->Render( m_appState );

		if( m_renderer->EndRender() != VK_SUCCESS )
		{
			Log::Error( "Failed to end  render" );
			break;
		}

		time = newTime;
	}

	if( m_window )
	{
		glfwDestroyWindow( m_window );
		m_window = nullptr;
	}
	glfwTerminate();
}

void Application::SetData( CmfContent* data )
{
	if( !data )
	{
		Log::Error( "Invalid CMF data. Ignoring" );
		return;
	}
	m_appState.cmfContent.SetValue( data );
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
			m_appState.windowSize.SetValue( std::make_pair( width, height ) );
		}
	}
}

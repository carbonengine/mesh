// Copyright © 2025 CCP ehf.

#include "application.h"

#include <algorithm>
#include "rendering/vulkan/shadercache.h"
#include "rendering/vulkan/vulkanerrors.h"
#include "viewerIcon.h"

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
	SetCarbonMeshViewerWindowIcon( m_window );

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
	m_renderer = std::make_shared<Renderer>();
	m_renderer->Resize( width, height );

	// if we set value regularly we will trigger a callback, which is not needed
	m_appState.windowSize.SetValueNoCallback( std::make_pair( width, height ) );

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

	ON_ERROR_LOG_AND_RETURN( ShaderCache::InitializeShaders( m_renderer.get() ), "Failed to initialize shaders" );
	glfwSetWindowUserPointer( m_window, this );

	m_sceneRenderer = std::make_unique<SceneRenderer>( m_renderer );
	ON_ERROR_LOG_AND_RETURN( m_sceneRenderer->Initialize( m_appState ), "Faied to initialize scene renderer" );

	m_orientationGizmoRenderer = std::make_unique<OrientationGizmoRenderer>( m_renderer );
	m_orientationGizmoRenderer->Initialize( m_appState );

	m_uiRenderer = std::make_unique<UIRenderer>( m_renderer );
	m_uiRenderer->Initialize( m_window, m_appState );

	m_camera.Initialize( m_appState );

	// initialize input handler
	glfwSetDropCallback( m_window, []( GLFWwindow* window, int count, const char** paths ) {
		Application* app = reinterpret_cast<Application*>( glfwGetWindowUserPointer( window ) );
		if( app )
		{
			for( int i = 0; i < count; ++i )
			{
				app->LoadCmfFile( std::string( paths[i] ) );
			}
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
	m_appState.windowSize.RegisterCallback( [this]( std::pair<uint32_t, uint32_t> size, AppState& ) {
		auto [width, height] = size;

		if( width * height == 0 )
		{
			m_minimized = true;
			return;
		}
		m_minimized = false;

		m_renderer->PreResize();
		m_renderer->ReleaseSurface();
		glfwCreateWindowSurface( m_renderer->GetVulkanInstance(), m_window, nullptr, m_renderer->GetSurface() );
		m_renderer->Resize( width, height );
	} );

	m_valid = true;
}

void Application::Run()
{
	if( !m_renderer || !m_valid )
	{
		return;
	}
	float time = (float)glfwGetTime();

	while( !glfwWindowShouldClose( m_window ) && !m_appState.exitRequested.GetValue() )
	{
		m_appState.CallStateCallbacks();

		// Poll for and process events
		glfwPollEvents();

		float newTime = (float)glfwGetTime();
		if( !m_minimized )
		{
			m_camera.Update( newTime - time );

			m_sceneRenderer->Update( m_appState, m_camera );
			if( m_renderer->BeginCompute() != VK_SUCCESS )
			{
				Log::Error( "Failed to begin compute step" );
				break;
			}

			m_sceneRenderer->PrePass();

			if( m_renderer->EndCompute() != VK_SUCCESS )
			{
				Log::Error( "Failed to end compute step" );
				break;
			}

			if( m_renderer->BeginRender() != VK_SUCCESS )
			{
				Log::Error( "Failed to begin render" );
				break;
			}
			m_uiRenderer->BeginFrame();

			m_sceneRenderer->Render( m_appState, m_camera );
			m_orientationGizmoRenderer->Render( m_appState, m_camera );

			m_uiRenderer->Update( m_appState );
			m_uiRenderer->Render( m_appState );

			if( m_renderer->EndRender() != VK_SUCCESS )
			{
				Log::Error( "Failed to end  render" );
				break;
			}
		}

		time = newTime;
	}
	auto logicalDevice = m_renderer->GetDevice()->GetLogicalDevice();
	vkDeviceWaitIdle( logicalDevice );
	m_orientationGizmoRenderer.release();

	m_sceneRenderer.release();
	m_uiRenderer.release();

	ShaderCache::ReleaseShaders( m_renderer.get() );
	m_renderer.reset();

	if( m_window )
	{
		glfwDestroyWindow( m_window );
		m_window = nullptr;
	}
	glfwTerminate();
}

void Application::LoadCmfFile( const std::string& path )
{
	// check if we are loading a full model or an animation/pose file
	std::shared_ptr<CmfContent> data = CmfContentLoader::LoadContentFromFile( path );
	auto currentData = m_appState.cmfContent.GetValue();
	if( currentData != nullptr && data && data->m_cmfData->meshes.empty() )
	{
		Log::Info( "Applying animation/pose file %s", path.c_str() );
		m_appState.modelState.animationOverrides.AddState( data );
	}
	else
	{
		Log::Info( "Applying model file %s", path.c_str() );
		// load the cmf content from the path and set it in the app state
		m_appState.cmfPath.SetValue( path );
		m_appState.modelState.activeAnimationOwner.SetValue( data );
		m_appState.cmfContent.ForceSetValue( data );
	}
}

void Application::Resize( uint32_t width, uint32_t height )
{
	if( m_renderer )
	{
		m_appState.windowSize.SetValue( std::make_pair( width, height ) );
	}
}

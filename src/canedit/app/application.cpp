// Copyright © 2026 CCP ehf.

#include "application.h"

#include "fileDialogs.h"
#include "nodeInfo.h"

#include "rendering/vulkan/shadercache.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <filesystem>

// ImGui uses C-style variadic functions for formatted text
// NOLINTBEGIN(cppcoreguidelines-pro-type-vararg)

namespace canedit
{
namespace
{

constexpr int INITIAL_WIDTH = 1920;
constexpr int INITIAL_HEIGHT = 1080;
constexpr float BASE_FONT_SIZE = 15.f;
constexpr float MIN_UI_SCALE = 0.5f;
constexpr float MAX_UI_SCALE = 3.f;
constexpr float UI_SCALE_STEP = 0.1f;
constexpr float SPLITTER_THICKNESS = 6.f;
constexpr float MIN_PANEL_SIZE = 120.f;
constexpr float MIN_CANVAS_WIDTH = 200.f;
/** Seconds a status message stays visible. */
constexpr double STATUS_DURATION = 8.0;
constexpr float CLEAR_COLOR = 0.08f;
constexpr float FRAME_ROUNDING = 3.f;
/** Padding of the host window around the panels, unscaled. */
constexpr float HOST_PADDING = 4.f;
constexpr float SETTINGS_WINDOW_WIDTH = 640.f;
constexpr float SETTINGS_WINDOW_HEIGHT = 320.f;
/** Width of the directory column of the prefix table relative to the prefix column. */
constexpr float DIRECTORY_COLUMN_WEIGHT = 4.f;
/** Frame padding is applied on both sides of a widget. */
constexpr float TWO_SIDES = 2.f;
constexpr ImVec4 ERROR_TEXT_COLOR = { 1.f, 0.45f, 0.4f, 1.f };

Application* GetApplication( GLFWwindow* window )
{
	return static_cast<Application*>( glfwGetWindowUserPointer( window ) );
}

std::string GetDocumentName( const Document& document )
{
	const auto& path = document.GetFilePath();
	return path.empty() ? std::string( "Untitled" ) : path.filename().u8string();
}

bool HasExtension( const std::string& path, const char* extension )
{
	auto actual = std::filesystem::u8path( path ).extension().u8string();
	std::transform( actual.begin(), actual.end(), actual.begin(), []( char c ) { return static_cast<char>( std::tolower( static_cast<unsigned char>( c ) ) ); } );
	return actual == extension;
}

}

Application::Application()
{
	m_loader = std::make_unique<FileAssetLoader>( m_resolver );
	m_document = std::make_unique<Document>( *m_loader );
	m_session = std::make_unique<EditorSession>( *m_document );
}

Application::~Application()
{
	Shutdown();
}

bool Application::Initialize()
{
	std::string error;
	if( !LoadSettings( GetDefaultSettingsPath(), m_settings, error ) )
	{
		Log::Warning( "%s", error.c_str() );
	}
	m_resolver.SetPrefixes( m_settings.prefixes );

	if( !glfwInit() )
	{
		Log::Error( "Failed to initialize GLFW" );
		return false;
	}
	if( !glfwVulkanSupported() )
	{
		Log::Error( "GLFW was not built with Vulkan support or no Vulkan loader was found" );
		return false;
	}
	glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
	m_window = glfwCreateWindow( INITIAL_WIDTH, INITIAL_HEIGHT, "CanEdit", nullptr, nullptr );
	if( m_window == nullptr )
	{
		Log::Error( "Failed to create the window" );
		return false;
	}
	glfwSetWindowUserPointer( m_window, this );

	float contentScaleX = 1.f;
	float contentScaleY = 1.f;
	glfwGetWindowContentScale( m_window, &contentScaleX, &contentScaleY );
	m_uiScale = std::clamp( m_settings.uiScale > 0.f ? m_settings.uiScale : contentScaleX, MIN_UI_SCALE, MAX_UI_SCALE );

	int width = 0;
	int height = 0;
	glfwGetFramebufferSize( m_window, &width, &height );

	uint32_t extensionCount = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions( &extensionCount );
	// NOLINTNEXTLINE(misc-const-correctness): extended on Apple platforms
	std::vector<const char*> extensions( glfwExtensions, glfwExtensions + extensionCount ); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic): GLFW returns a C array
#ifdef APPLE
	extensions.push_back( VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME );
#endif
	m_renderer = std::make_shared<Renderer>();
	m_renderer->Resize( static_cast<uint32_t>( width ), static_cast<uint32_t>( height ) );
	if( m_renderer->CreateInstance( extensions, "CanEdit" ) != VK_SUCCESS )
	{
		Log::Error( "Failed to create the Vulkan instance" );
		return false;
	}
	if( glfwCreateWindowSurface( m_renderer->GetVulkanInstance(), m_window, nullptr, m_renderer->GetSurface() ) != VK_SUCCESS )
	{
		Log::Error( "Failed to create the window surface" );
		return false;
	}
	m_renderer->Initialize();
	if( !m_renderer->IsValid() )
	{
		Log::Error( "Failed to initialize the renderer" );
		return false;
	}
	if( ShaderCache::InitializeShaders( m_renderer.get() ) != VK_SUCCESS )
	{
		Log::Error( "Failed to initialize the shaders" );
		return false;
	}
	m_shadersInitialized = true;

	ImGuiLayer::CreateContext();
	ImGui::GetIO().IniFilename = nullptr;
	ApplyUiScale( m_uiScale );
	m_imgui = std::make_unique<ImGuiLayer>( m_renderer );
	if( m_imgui->InitializeBackend( m_window, static_cast<uint32_t>( width ), static_cast<uint32_t>( height ) ) != VK_SUCCESS )
	{
		return false;
	}
	m_imgui->SetClearColor( CLEAR_COLOR, CLEAR_COLOR, CLEAR_COLOR );

	m_preview = std::make_unique<PreviewController>( m_renderer, *m_loader );
	m_context.resolver = &m_resolver;
	m_context.loader = m_loader.get();
	m_context.document = m_document.get();
	m_context.session = m_session.get();
	m_context.settings = &m_settings;
	m_context.preview = m_preview.get();
	m_canvas = std::make_unique<GraphCanvas>( m_context );

	m_document->New();
	UpdateTitle();

	glfwSetFramebufferSizeCallback( m_window, []( GLFWwindow* window, int, int ) { GetApplication( window )->m_framebufferResized = true; } );
	glfwSetWindowCloseCallback( m_window, []( GLFWwindow* window ) {
		// Closing is decided in the main loop, after asking about unsaved changes
		glfwSetWindowShouldClose( window, GLFW_FALSE );
		GetApplication( window )->m_closeRequested = true;
	} );
	glfwSetDropCallback( m_window, []( GLFWwindow* window, int count, const char** paths ) {
		const std::vector<std::string> files( paths, paths + count ); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic): GLFW passes a C array
		GetApplication( window )->OnDrop( files );
	} );
	return true;
}

void Application::Shutdown()
{
	if( m_renderer && m_renderer->GetDevice() != nullptr )
	{
		vkDeviceWaitIdle( m_renderer->GetDevice()->GetLogicalDevice() );
	}
	std::string error;
	if( m_window != nullptr && !SaveSettings( GetDefaultSettingsPath(), m_settings, error ) )
	{
		Log::Warning( "%s", error.c_str() );
	}
	m_canvas.reset();
	m_preview.reset();
	m_imgui.reset();
	if( m_shadersInitialized )
	{
		ShaderCache::ReleaseShaders( m_renderer.get() );
		m_shadersInitialized = false;
	}
	m_renderer.reset();
	if( m_window != nullptr )
	{
		glfwDestroyWindow( m_window );
		m_window = nullptr;
		glfwTerminate();
	}
}

void Application::ApplyUiScale( float scale )
{
	auto& io = ImGui::GetIO();
	io.Fonts->Clear();
	ImFontConfig config;
	config.SizePixels = BASE_FONT_SIZE * scale;
	config.OversampleH = 2;
	config.OversampleV = 2;
	io.Fonts->AddFontDefault( &config );
	io.Fonts->Build();

	auto& style = ImGui::GetStyle();
	style = ImGuiStyle();
	ImGui::StyleColorsDark( &style );
	style.FrameRounding = FRAME_ROUNDING;
	style.WindowRounding = 0.f;
	style.ChildRounding = 0.f;
	style.ScaleAllSizes( scale );
	m_uiScale = scale;
}

void Application::OnFramebufferResize()
{
	m_framebufferResized = false;
	int width = 0;
	int height = 0;
	glfwGetFramebufferSize( m_window, &width, &height );
	m_minimized = width == 0 || height == 0;
	if( m_minimized )
	{
		return;
	}
	m_renderer->PreResize();
	m_renderer->ReleaseSurface();
	glfwCreateWindowSurface( m_renderer->GetVulkanInstance(), m_window, nullptr, m_renderer->GetSurface() );
	m_renderer->Resize( static_cast<uint32_t>( width ), static_cast<uint32_t>( height ) );
	m_imgui->SetRenderSize( static_cast<uint32_t>( width ), static_cast<uint32_t>( height ) );
}

void Application::OnDrop( const std::vector<std::string>& paths )
{
	for( const auto& path : paths )
	{
		if( HasExtension( path, ".can" ) )
		{
			OpenFile( path );
			return;
		}
		if( HasExtension( path, ".cmf" ) )
		{
			const ScopedEdit edit( *m_session );
			m_document->AddPreviewMesh( m_resolver.MakeReference( std::filesystem::u8path( path ) ) );
		}
	}
}

void Application::Run()
{
	double time = glfwGetTime();
	while( !m_quit )
	{
		glfwPollEvents();
		if( m_closeRequested )
		{
			m_closeRequested = false;
			m_quit = ConfirmDiscardChanges();
			continue;
		}
		if( m_framebufferResized )
		{
			OnFramebufferResize();
		}
		if( m_minimized )
		{
			glfwWaitEvents();
			continue;
		}
		const double now = glfwGetTime();
		const auto deltaTime = static_cast<float>( now - time );
		time = now;

		if( m_pendingUiScale > 0.f && m_pendingUiScale != m_uiScale )
		{
			ApplyUiScale( m_pendingUiScale );
			ImGuiLayer::UpdateFontsTexture();
		}
		m_pendingUiScale = 0.f;
		m_preview->BeginFrame();
		ImGuiLayer::BeginFrame();
		DrawUi();
		// An edit can't stay open once nothing is being edited, for example when the edited item disappeared
		while( m_session->IsEditing() && !ImGui::IsAnyItemActive() && !m_canvas->IsInteracting() && !ImGui::IsMouseDown( ImGuiMouseButton_Left ) )
		{
			m_session->EndEdit();
		}
		m_preview->Update( deltaTime, *m_document );

		if( m_renderer->BeginCompute() != VK_SUCCESS )
		{
			Log::Error( "Failed to begin the compute pass" );
			break;
		}
		m_preview->PrePass();
		if( m_renderer->EndCompute() != VK_SUCCESS || m_renderer->BeginRender() != VK_SUCCESS )
		{
			Log::Error( "Failed to begin rendering" );
			break;
		}
		m_preview->Render();
		m_imgui->Render();
		if( m_renderer->EndRender() != VK_SUCCESS )
		{
			Log::Error( "Failed to end rendering" );
			break;
		}
		UpdateTitle();
	}
}

void Application::UpdateTitle()
{
	auto title = GetDocumentName( *m_document ) + ( m_document->IsModified() ? "*" : "" ) + " - CanEdit";
	if( title != m_title )
	{
		m_title = std::move( title );
		glfwSetWindowTitle( m_window, m_title.c_str() );
	}
}

bool Application::ConfirmDiscardChanges()
{
	if( !m_document->IsModified() )
	{
		return true;
	}
	switch( dialogs::AskSaveChanges( GetDocumentName( *m_document ) ) )
	{
	case dialogs::Answer::Yes:
		return SaveDocument( false );
	case dialogs::Answer::No:
		return true;
	case dialogs::Answer::Cancel:
		break;
	}
	return false;
}

void Application::NewDocument()
{
	if( !ConfirmDiscardChanges() )
	{
		return;
	}
	m_document->New();
	m_session->Reset();
	m_preview->Reload();
}

void Application::OpenDocument()
{
	if( !ConfirmDiscardChanges() )
	{
		return;
	}
	if( const auto path = dialogs::OpenNetwork() )
	{
		LoadDocument( *path );
	}
}

void Application::OpenFile( const std::string& path )
{
	if( ConfirmDiscardChanges() )
	{
		LoadDocument( path );
	}
}

void Application::LoadDocument( const std::string& path )
{
	std::string error;
	if( !m_document->LoadFile( std::filesystem::u8path( path ), error ) )
	{
		dialogs::ShowError( error );
		m_settings.recentFiles.erase( std::remove( m_settings.recentFiles.begin(), m_settings.recentFiles.end(), path ), m_settings.recentFiles.end() );
		return;
	}
	m_session->Reset();
	m_preview->Reload();
	m_settings.AddRecentFile( path );
	m_context.SetStatus( "Opened " + path );
}

bool Application::SaveDocument( bool askForPath )
{
	auto path = m_document->GetFilePath();
	if( askForPath || path.empty() )
	{
		const auto chosen = dialogs::SaveNetwork( path.u8string() );
		if( !chosen )
		{
			return false;
		}
		path = std::filesystem::u8path( *chosen );
	}
	std::string error;
	if( !m_document->SaveFile( path, error ) )
	{
		dialogs::ShowError( error );
		return false;
	}
	m_settings.AddRecentFile( path.u8string() );
	m_context.SetStatus( "Saved " + path.u8string() );
	return true;
}

void Application::Undo()
{
	if( !m_session->Undo() )
	{
		m_context.SetStatus( "Nothing to undo" );
	}
}

void Application::Redo()
{
	if( !m_session->Redo() )
	{
		m_context.SetStatus( "Nothing to redo" );
	}
}

void Application::HandleGlobalShortcuts()
{
	if( ImGui::Shortcut( ImGuiMod_Ctrl | ImGuiKey_N, ImGuiInputFlags_RouteGlobal ) )
	{
		NewDocument();
	}
	if( ImGui::Shortcut( ImGuiMod_Ctrl | ImGuiKey_O, ImGuiInputFlags_RouteGlobal ) )
	{
		OpenDocument();
	}
	if( ImGui::Shortcut( ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteGlobal ) )
	{
		SaveDocument( false );
	}
	if( ImGui::Shortcut( ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S, ImGuiInputFlags_RouteGlobal ) )
	{
		SaveDocument( true );
	}
	if( !ImGui::GetIO().WantTextInput )
	{
		if( ImGui::Shortcut( ImGuiMod_Ctrl | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal ) )
		{
			Undo();
		}
		if( ImGui::Shortcut( ImGuiMod_Ctrl | ImGuiKey_Y, ImGuiInputFlags_RouteGlobal ) || ImGui::Shortcut( ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal ) )
		{
			Redo();
		}
	}
}

void Application::DrawUi()
{
	HandleGlobalShortcuts();
	DrawMenuBar();
	DrawLayout();
	DrawSettingsWindow();
}

void Application::DrawMenuBar()
{
	if( !ImGui::BeginMainMenuBar() )
	{
		return;
	}
	DrawFileMenu();
	DrawEditMenu();
	DrawViewMenu();
	if( ImGui::BeginMenu( "Settings" ) )
	{
		if( ImGui::MenuItem( "Path Prefixes..." ) )
		{
			m_editedPrefixes = m_settings.prefixes;
			m_showSettings = true;
		}
		ImGui::EndMenu();
	}
	ImGui::EndMainMenuBar();
}

void Application::DrawFileMenu()
{
	if( !ImGui::BeginMenu( "File" ) )
	{
		return;
	}
	if( ImGui::MenuItem( "New", "Ctrl+N" ) )
	{
		NewDocument();
	}
	if( ImGui::MenuItem( "Open...", "Ctrl+O" ) )
	{
		OpenDocument();
	}
	if( ImGui::BeginMenu( "Open Recent", !m_settings.recentFiles.empty() ) )
	{
		// Copy: opening a file changes the list
		const auto recentFiles = m_settings.recentFiles;
		for( const auto& file : recentFiles )
		{
			if( ImGui::MenuItem( file.c_str() ) )
			{
				OpenFile( file );
			}
		}
		ImGui::EndMenu();
	}
	ImGui::Separator();
	if( ImGui::MenuItem( "Save", "Ctrl+S" ) )
	{
		SaveDocument( false );
	}
	if( ImGui::MenuItem( "Save As...", "Ctrl+Shift+S" ) )
	{
		SaveDocument( true );
	}
	ImGui::Separator();
	if( ImGui::MenuItem( "Reload Assets" ) )
	{
		m_loader->Flush();
		m_session->Reload();
		m_preview->Reload();
	}
	ImGui::Separator();
	if( ImGui::MenuItem( "Exit" ) )
	{
		m_closeRequested = true;
	}
	ImGui::EndMenu();
}

void Application::DrawEditMenu()
{
	if( !ImGui::BeginMenu( "Edit" ) )
	{
		return;
	}
	const bool canEdit = m_session->CanEdit();
	if( ImGui::MenuItem( "Undo", "Ctrl+Z", false, m_session->CanUndo() ) )
	{
		Undo();
	}
	if( ImGui::MenuItem( "Redo", "Ctrl+Y", false, m_session->CanRedo() ) )
	{
		Redo();
	}
	ImGui::Separator();
	const bool hasSelection = !m_session->GetSelection().empty();
	if( ImGui::MenuItem( "Cut", "Ctrl+X", false, canEdit && hasSelection ) )
	{
		m_canvas->CopyToClipboard( true );
	}
	if( ImGui::MenuItem( "Copy", "Ctrl+C", false, hasSelection ) )
	{
		m_canvas->CopyToClipboard( false );
	}
	if( ImGui::MenuItem( "Paste", "Ctrl+V", false, canEdit ) )
	{
		m_canvas->PasteFromClipboard();
	}
	if( ImGui::MenuItem( "Delete", "Del", false, canEdit && ( hasSelection || m_session->GetSelectedEdge() ) ) )
	{
		m_session->DeleteSelection();
	}
	ImGui::Separator();
	if( ImGui::MenuItem( "Select All", "Ctrl+A" ) )
	{
		m_session->SelectAll();
	}
	if( ImGui::MenuItem( "Auto Arrange", nullptr, false, canEdit ) )
	{
		m_session->AutoLayout();
		m_canvas->FrameNodes( false );
	}
	ImGui::EndMenu();
}

void Application::DrawViewMenu()
{
	if( !ImGui::BeginMenu( "View" ) )
	{
		return;
	}
	if( ImGui::MenuItem( "Frame All", "Home" ) )
	{
		m_canvas->FrameNodes( false );
	}
	if( ImGui::MenuItem( "Frame Selection", "F" ) )
	{
		m_canvas->FrameNodes( true );
	}
	if( ImGui::MenuItem( "Leave Subgraph", "Backspace", false, !m_session->GetGraphPath().empty() ) )
	{
		m_session->LeaveSubgraph();
	}
	ImGui::Separator();
	float scale = m_uiScale;
	if( ImGui::MenuItem( "Larger UI" ) )
	{
		scale += UI_SCALE_STEP;
		m_settings.uiScale = scale;
	}
	if( ImGui::MenuItem( "Smaller UI" ) )
	{
		scale -= UI_SCALE_STEP;
		m_settings.uiScale = scale;
	}
	if( ImGui::MenuItem( "Default UI Size" ) )
	{
		float contentScaleX = 1.f;
		float contentScaleY = 1.f;
		glfwGetWindowContentScale( m_window, &contentScaleX, &contentScaleY );
		scale = contentScaleX;
		m_settings.uiScale = 0.f;
	}
	scale = std::clamp( scale, MIN_UI_SCALE, MAX_UI_SCALE );
	if( scale != m_uiScale )
	{
		// Fonts can't change during a frame, the scale is applied before the next one
		m_pendingUiScale = scale;
	}
	ImGui::EndMenu();
}

void Application::Splitter( const char* id, bool vertical, float length, float& size, float minSize, float maxSize ) const
{
	const auto thickness = SPLITTER_THICKNESS * m_uiScale;
	ImGui::InvisibleButton( id, vertical ? ImVec2{ thickness, length } : ImVec2{ length, thickness } );
	if( ImGui::IsItemHovered() || ImGui::IsItemActive() )
	{
		ImGui::SetMouseCursor( vertical ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS );
	}
	if( ImGui::IsItemActive() )
	{
		const auto delta = vertical ? ImGui::GetIO().MouseDelta.x : ImGui::GetIO().MouseDelta.y;
		size = std::clamp( size + delta / m_uiScale, minSize, std::max( minSize, maxSize ) );
	}
}

void Application::DrawBreadcrumb()
{
	auto& session = *m_session;
	if( ImGui::SmallButton( "Root" ) )
	{
		session.OpenGraph( {} );
	}
	const auto path = session.GetGraphPath();
	GraphPath partial;
	for( size_t i = 0; i < path.size(); ++i )
	{
		const auto* parent = m_document->FindGraph( partial );
		partial.push_back( path[i] );
		ImGui::SameLine();
		ImGui::TextDisabled( ">" );
		ImGui::SameLine();
		ImGui::PushID( static_cast<int>( i ) );
		const auto name = parent != nullptr ? GetNodeTitle( *parent, path[i] ) : std::string( "?" );
		if( ImGui::SmallButton( name.c_str() ) )
		{
			session.OpenGraph( partial );
		}
		ImGui::PopID();
	}
}

void Application::DrawStatusBar()
{
	if( !m_context.status.empty() && ImGui::GetTime() - m_context.statusTime < STATUS_DURATION )
	{
		const auto color = m_context.statusIsError ? ERROR_TEXT_COLOR : ImVec4( 0.8f, 0.8f, 0.8f, 1.f );
		ImGui::TextColored( color, "%s", m_context.status.c_str() );
	}
	else
	{
		const auto errors = m_document->GetNetwork().GetErrors().size();
		const auto warnings = m_document->GetNetwork().GetWarnings().size() + m_loader->GetErrors().size();
		const auto path = m_document->GetFilePath().empty() ? std::string( "Untitled" ) : m_document->GetFilePath().u8string();
		ImGui::TextDisabled( "%s | %zu errors, %zu warnings | %s", path.c_str(), errors, warnings, m_preview->IsNetworkReady() ? "network ready" : "network not ready" );
	}
}

void Application::DrawLayout()
{
	const auto* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos( viewport->WorkPos );
	ImGui::SetNextWindowSize( viewport->WorkSize );
	constexpr auto hostFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollWithMouse;
	ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, { HOST_PADDING * m_uiScale, HOST_PADDING * m_uiScale } );
	ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, { 0.f, 0.f } );
	ImGui::Begin( "##host", nullptr, hostFlags );
	ImGui::PopStyleVar( 2 );

	const auto scale = m_uiScale;
	const auto& style = ImGui::GetStyle();
	const auto spacing = style.ItemSpacing;
	const auto statusHeight = ImGui::GetTextLineHeight();
	const auto timelineHeight = ImGui::GetFrameHeight() + style.WindowPadding.y * 2.f + 2.f;
	const auto available = ImGui::GetContentRegionAvail();
	const auto splitter = SPLITTER_THICKNESS * scale;
	// Rows: main area, splitter gap, time line and status line, separated by item spacing
	const auto mainHeight = std::max( available.y - statusHeight - timelineHeight - splitter - 3.f * spacing.y, MIN_PANEL_SIZE * scale );
	// Columns: left, splitter, center, splitter, right, separated by item spacing
	const auto columnsWidth = available.x - 2.f * splitter - 4.f * spacing.x;
	const auto maxSide = ( columnsWidth - MIN_CANVAS_WIDTH * scale ) / scale;
	auto& settings = m_settings;
	settings.leftPanelWidth = std::clamp( settings.leftPanelWidth, MIN_PANEL_SIZE, std::max( MIN_PANEL_SIZE, maxSide - settings.rightPanelWidth ) );
	settings.rightPanelWidth = std::clamp( settings.rightPanelWidth, MIN_PANEL_SIZE, std::max( MIN_PANEL_SIZE, maxSide - settings.leftPanelWidth ) );
	const auto leftWidth = settings.leftPanelWidth * scale;
	const auto rightWidth = settings.rightPanelWidth * scale;
	const auto centerWidth = std::max( columnsWidth - leftWidth - rightWidth, 1.f );
	const auto panelFlags = ImGuiChildFlags_Borders;

	// Left column: palette and properties
	ImGui::BeginChild( "##left", { leftWidth, mainHeight } );
	{
		const auto height = ImGui::GetContentRegionAvail().y - 2.f * spacing.y;
		settings.paletteHeight = std::clamp( settings.paletteHeight, MIN_PANEL_SIZE, std::max( MIN_PANEL_SIZE, ( height - splitter ) / scale - MIN_PANEL_SIZE ) );
		ImGui::BeginChild( "Palette", { 0.f, settings.paletteHeight * scale }, panelFlags );
		ImGui::SeparatorText( "Nodes" );
		m_palette.Draw( m_context, *m_canvas );
		ImGui::EndChild();
		Splitter( "##paletteSplitter", false, leftWidth, settings.paletteHeight, MIN_PANEL_SIZE, ( height - splitter ) / scale - MIN_PANEL_SIZE );
		ImGui::BeginChild( "Properties", { 0.f, 0.f }, panelFlags );
		ImGui::SeparatorText( "Properties" );
		m_properties.Draw( m_context );
		ImGui::EndChild();
	}
	ImGui::EndChild();
	ImGui::SameLine();
	Splitter( "##leftSplitter", true, mainHeight, settings.leftPanelWidth, MIN_PANEL_SIZE, maxSide - settings.rightPanelWidth );
	ImGui::SameLine();

	// Center: breadcrumb and graph
	ImGui::BeginChild( "##center", { centerWidth, mainHeight }, panelFlags, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse );
	DrawBreadcrumb();
	ImGui::Spacing();
	m_canvas->Draw();
	ImGui::EndChild();
	ImGui::SameLine();
	{
		// Dragging the right splitter changes the width of the right column, which grows to the left
		float invertedWidth = -settings.rightPanelWidth;
		Splitter( "##rightSplitter", true, mainHeight, invertedWidth, -( maxSide - settings.leftPanelWidth ), -MIN_PANEL_SIZE );
		settings.rightPanelWidth = -invertedWidth;
	}
	ImGui::SameLine();

	// Right column: preview, parameters and problems
	ImGui::BeginChild( "##right", { rightWidth, mainHeight } );
	{
		const auto height = ImGui::GetContentRegionAvail().y - 4.f * spacing.y;
		const auto maxPreview = ( height - 2.f * splitter ) / scale - settings.parametersHeight - MIN_PANEL_SIZE;
		settings.previewHeight = std::clamp( settings.previewHeight, MIN_PANEL_SIZE, std::max( MIN_PANEL_SIZE, maxPreview ) );
		ImGui::BeginChild( "Preview", { 0.f, settings.previewHeight * scale }, panelFlags, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse );
		m_preview->DrawViewport();
		ImGui::EndChild();
		Splitter( "##previewSplitter", false, rightWidth, settings.previewHeight, MIN_PANEL_SIZE, maxPreview );
		const auto maxParameters = ( height - 2.f * splitter ) / scale - settings.previewHeight - MIN_PANEL_SIZE;
		settings.parametersHeight = std::clamp( settings.parametersHeight, MIN_PANEL_SIZE, std::max( MIN_PANEL_SIZE, maxParameters ) );
		ImGui::BeginChild( "Parameters", { 0.f, settings.parametersHeight * scale }, panelFlags );
		ImGui::SeparatorText( "Parameters" );
		m_parameters.Draw( m_context );
		ImGui::EndChild();
		Splitter( "##parametersSplitter", false, rightWidth, settings.parametersHeight, MIN_PANEL_SIZE, maxParameters );
		ImGui::BeginChild( "Problems", { 0.f, 0.f }, panelFlags );
		ImGui::SeparatorText( "Problems and Assets" );
		AssetsPanel::Draw( m_context );
		ImGui::EndChild();
	}
	ImGui::EndChild();

	// Bottom: time line and status
	ImGui::Dummy( { 0.f, splitter } );
	ImGui::BeginChild( "Timeline", { 0.f, timelineHeight }, panelFlags, ImGuiWindowFlags_NoScrollbar );
	m_preview->DrawTimeline();
	ImGui::EndChild();
	DrawStatusBar();
	ImGui::End();
}

void Application::DrawSettingsWindow()
{
	if( !m_showSettings )
	{
		return;
	}
	ImGui::SetNextWindowSize( { SETTINGS_WINDOW_WIDTH * m_uiScale, SETTINGS_WINDOW_HEIGHT * m_uiScale }, ImGuiCond_FirstUseEver );
	if( !ImGui::Begin( "Path Prefixes", &m_showSettings, ImGuiWindowFlags_NoCollapse ) )
	{
		ImGui::End();
		return;
	}
	ImGui::TextWrapped( "Asset references of the form 'name:/path/file.cmf' resolve to files in the directory of the prefix. Other references are file paths." );
	std::optional<size_t> removed;
	std::string error;
	if( ImGui::BeginTable( "prefixes", 3, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg ) )
	{
		ImGui::TableSetupColumn( "Prefix", ImGuiTableColumnFlags_WidthStretch, 1.f );
		ImGui::TableSetupColumn( "Directory", ImGuiTableColumnFlags_WidthStretch, DIRECTORY_COLUMN_WEIGHT );
		ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_WidthFixed );
		ImGui::TableHeadersRow();
		for( size_t i = 0; i < m_editedPrefixes.size(); ++i )
		{
			DrawPrefixRow( i, removed, error );
		}
		ImGui::EndTable();
	}
	if( removed )
	{
		m_editedPrefixes.erase( m_editedPrefixes.begin() + static_cast<std::ptrdiff_t>( *removed ) );
	}
	if( ImGui::Button( "Add Prefix" ) )
	{
		m_editedPrefixes.push_back( { "res", "" } );
	}
	if( !error.empty() )
	{
		ImGui::TextColored( ERROR_TEXT_COLOR, "%s", error.c_str() );
	}
	ImGui::Separator();
	ImGui::BeginDisabled( !error.empty() );
	if( ImGui::Button( "Apply" ) )
	{
		ApplyEditedPrefixes();
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if( ImGui::Button( "Cancel" ) )
	{
		m_showSettings = false;
	}
	ImGui::End();
}

void Application::DrawPrefixRow( size_t index, std::optional<size_t>& removed, std::string& error )
{
	auto& prefix = m_editedPrefixes[index];
	ImGui::PushID( static_cast<int>( index ) );
	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::SetNextItemWidth( -1.f );
	ImGui::InputText( "##name", &prefix.name );
	ImGui::TableNextColumn();
	ImGui::SetNextItemWidth( -ImGui::CalcTextSize( "Browse" ).x - ImGui::GetStyle().FramePadding.x * TWO_SIDES - ImGui::GetStyle().ItemSpacing.x );
	ImGui::InputText( "##directory", &prefix.directory );
	ImGui::SameLine();
	if( ImGui::Button( "Browse" ) )
	{
		if( const auto directory = dialogs::SelectDirectory( "Prefix Directory", prefix.directory ) )
		{
			prefix.directory = *directory;
		}
	}
	ImGui::TableNextColumn();
	if( ImGui::Button( "Remove" ) )
	{
		removed = index;
	}
	const auto nameError = PathResolver::ValidatePrefixName( prefix.name );
	const auto duplicates = std::count_if( m_editedPrefixes.begin(), m_editedPrefixes.end(), [&prefix]( const PathPrefix& other ) { return other.name == prefix.name; } );
	if( error.empty() && !nameError.empty() )
	{
		error = "'" + prefix.name + "': " + nameError;
	}
	else if( error.empty() && duplicates > 1 )
	{
		error = "Prefix '" + prefix.name + "' is defined more than once";
	}
	ImGui::PopID();
}

void Application::ApplyEditedPrefixes()
{
	m_settings.prefixes = m_editedPrefixes;
	m_resolver.SetPrefixes( m_settings.prefixes );
	std::string saveError;
	if( !SaveSettings( GetDefaultSettingsPath(), m_settings, saveError ) )
	{
		m_context.SetStatus( saveError, true );
	}
	m_loader->Flush();
	m_session->Reload();
	m_preview->Reload();
	m_showSettings = false;
}

}

// NOLINTEND(cppcoreguidelines-pro-type-vararg)

// Copyright © 2026 CCP ehf.

#include "uiRenderer.h"

#include <algorithm>
#include <faBinary.h>
#include <faLookup.h>
#include "appState.h"
#include "uiConsts.h"
#include "uiCustomWidgets.h"
#include "uiSettings.h"
#include "rendering/vulkan/vulkanerrors.h"


// ImGui is using a lot of variadic functions for text formatting, so we disable the cppcoreguidelines-pro-type-vararg lint for this file
// NOLINTBEGIN(cppcoreguidelines-pro-type-vararg)


UIRenderer::UIRenderer( std::shared_ptr<const Renderer> renderer ) :
	m_renderer( renderer ),
	m_imguiLayer( renderer )
{
}

UIRenderer::~UIRenderer() = default;

void UIRenderer::Initialize( GLFWwindow* window, AppState& state )
{
	Log::Info( "Initializing UI Renderer" );
	// Setup Dear ImGui context
	ImGuiLayer::CreateContext();
	// settings must be loaded before the fonts are built
	UiSettings::Initialize( state );
	ApplyUiScale( state.uiScale.GetValue() );

	auto [width, height] = state.windowSize.GetValue();
	CR( m_imguiLayer.InitializeBackend( window, width, height ) );

	state.windowSize.RegisterCallback( [this]( std::pair<uint32_t, uint32_t> size, AppState& appState ) {
		auto [width, height] = size;
		m_imguiLayer.SetRenderSize( width, height );
	} );

	state.cmfContent.RegisterCallback( [this]( std::shared_ptr<CmfContent> content, AppState& appState ) {
		if( content == nullptr )
		{
			m_loadStatus = LoadStatus::FAILED;
		}
		else
		{
			m_loadStatus = LoadStatus::SUCCESSFUL;
		}
	} );

	state.uiScale.RegisterCallback( [this]( float scale, AppState& appState ) {
		ApplyUiScale( scale );
		ImGuiLayer::UpdateFontsTexture();
		UiSettings::MarkDirty();
	} );
}


void UIRenderer::ApplyUiScale( float scale )
{
	scale = std::clamp( scale, UiConsts::MIN_UI_SCALE, UiConsts::MAX_UI_SCALE );
	Log::Info( "Applying ui scale %.2fx", scale );

	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->Clear();

	ImFontConfig defaultConfig;
	defaultConfig.SizePixels = UiConsts::BASE_FONT_SIZE * scale;
	defaultConfig.OversampleH = 1;
	defaultConfig.OversampleV = 1;
	defaultConfig.PixelSnapH = true;
	io.Fonts->AddFontDefault( &defaultConfig );

	// add fontawesome icons
	static const ImWchar iconRanges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
	ImFontConfig iconConfig;
	iconConfig.MergeMode = true;
	iconConfig.FontDataOwnedByAtlas = false;
	io.Fonts->AddFontFromMemoryTTF( (void*)fa_solid_900_ttf_data, static_cast<int>( fa_solid_900_ttf_size ), UiConsts::BASE_FONT_AWESOME_SIZE * scale, &iconConfig, iconRanges );
	io.Fonts->Build();

	// ScaleAllSizes is cumulative, so always start from a fresh style
	ImGuiStyle& style = ImGui::GetStyle();
	style = ImGuiStyle();
	ImGui::StyleColorsDark( &style );
	style.ScaleAllSizes( scale );
}

void UIRenderer::BeginFrame()
{
	// Start the Dear ImGui frame
	ImGuiLayer::BeginFrame();
}

void UIRenderer::Render( AppState& appState )
{
	SetupUi( appState );
	m_imguiLayer.Render();
}

void UIRenderer::SetupUi( AppState& appState )
{
	m_menubar.Render( appState );
	if( appState.showUi.GetValue() )
	{
		m_generalWindow.Render( appState, UiConsts::MenuBarHeight(), UiConsts::AnimationPlayerHeight() );
		m_detailWindow.Render( appState, UiConsts::MenuBarHeight(), UiConsts::AnimationPlayerHeight() );
		m_animationPlayback.Render( appState );
	}
	SetupPopupWindows( appState );
}


void UIRenderer::SetupPopupWindows( AppState& appState )
{
	if( m_loadStatus == LoadStatus::FAILED )
	{
		ImGui::OpenPopup( "Error" );
		bool open = true;
		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos( center, ImGuiCond_Appearing, ImVec2( 0.5f, 0.5f ) );

		if( ImGui::BeginPopupModal( "Error", &open, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse ) )
		{
			auto filePath = appState.cmfPath.GetValue();
			ImGui::Text( "Failed to load" );
			ImGui::Text( "%s", filePath.c_str() );
			// At some point I can add the failure reason here...
			ImGui::Separator();
			ImGui::NewLine();
			ImVec2 availableSize = ImGui::GetContentRegionAvail();
			const ImVec2 buttonSize( UiConsts::Scaled( 100.0f ), UiConsts::Scaled( 24.0f ) );
			ImGui::SameLine( ( availableSize.x - buttonSize.x ) / 2.0f );

			if( ImGui::Button( "Ok", buttonSize ) )
			{
				m_loadStatus = LoadStatus::NOTHING_LOADED;
			}
			ImGui::EndPopup();
		}
	}
	else
	{
		m_loadStatus = LoadStatus::NOTHING_LOADED;
	}
}

void UIRenderer::Update( AppState& appstate )
{
	UpdateInputs( appstate );
}

void UIRenderer::UpdateInputs( AppState& appState )
{
	auto io = ImGui::GetIO();
	if( !io.WantCaptureMouse )
	{
		MouseState mouseState{};
		mouseState.position = { io.MousePos.x, io.MousePos.y };
		mouseState.previousPosition = { io.MousePosPrev.x, io.MousePosPrev.y };
		mouseState.wheelDelta = io.MouseWheel;
		mouseState.leftButton = io.MouseDown[0];
		mouseState.middleButton = io.MouseDown[2];
		mouseState.rightButton = io.MouseDown[1];
		appState.mouseState.SetValue( mouseState );
	}
	else
	{
		appState.mouseState.Reset();
	}
	// Handle ui keyboard shortcuts
	if( ImGui::IsKeyChordPressed( ImGuiMod_Ctrl | ImGuiKey_O ) )
	{
		// file open
		auto filePath = ImGui::OpenCmfFileDialog();
		if( filePath != nullptr )
		{
			appState.cmfLoadRequest.SetValue( filePath );
		}
	}
	const auto content = appState.cmfContent.GetValue();
	if( content != nullptr )
	{
		if( ImGui::IsKeyChordPressed( ImGuiMod_Ctrl | ImGuiKey_F ) )
		{
			// focus camera on model
			appState.cameraFocus.ForceSetValue( content->GetBoundingSphere() );
			appState.cameraTrigger.ForceSetValue( CameraTrigger::Focus );
		}
	}
	if( ImGui::IsKeyChordPressed( ImGuiMod_Ctrl | ImGuiKey_F12 ) )
	{
		appState.showUi.SetValue( !appState.showUi.GetValue() );
	}
}


// NOLINTEND(cppcoreguidelines-pro-type-vararg)
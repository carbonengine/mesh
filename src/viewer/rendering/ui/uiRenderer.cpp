// Copyright © 2026 CCP ehf.

#include "uiRenderer.h"

#include <algorithm>
#include <faBinary.h>
#include <faLookup.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include "appState.h"
#include "uiConsts.h"
#include "uiCustomWidgets.h"
#include "../vulkan/vulkanerrors.h"


// ImGui is using a lot of variadic functions for text formatting, so we disable the cppcoreguidelines-pro-type-vararg lint for this file
// NOLINTBEGIN(cppcoreguidelines-pro-type-vararg)


UIRenderer::UIRenderer( std::shared_ptr<const Renderer> renderer ) :
	m_renderer( renderer ),
	m_graphicsCommandBuffer( renderer.get() )
{
}

UIRenderer::~UIRenderer()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui::DestroyContext();
}

static void check_vk_result( VkResult err )
{
	if( err == VK_SUCCESS )
	{
		return;
	}
	Log::Error( "[vulkan] Error: VkResult = %d\n", err );
	if( err < 0 )
	{
		abort();
	}
}

void UIRenderer::Initialize( GLFWwindow* window, AppState& state )
{
	Log::Info( "Initializing UI Renderer" );
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
	io.Fonts->AddFontDefault();

	static const ImWchar iconRanges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };

	ImFontConfig config;
	config.MergeMode = true;
	config.FontDataOwnedByAtlas = false;
	// add fontawesome icons
	io.Fonts->AddFontFromMemoryTTF( (void*)fa_solid_900_ttf_data, static_cast<int>( fa_solid_900_ttf_size ), UiConsts::FONT_AWESOME_SIZE, &config, iconRanges );
	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	Device* device = m_renderer->GetDevice();
	const Swapchain* swapchain = m_renderer->GetSwapchain();
	VkDevice logicalDevice = device->GetLogicalDevice();
	const VkAllocationCallbacks* allocator = m_renderer->GetAllocator();

	assert( device->GetQueueFamilyIndices().graphicsFamily.has_value() );

	VkDescriptorPoolSize pool_sizes[] = {
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE },
	};
	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 0;
	for( VkDescriptorPoolSize& pool_size : pool_sizes )
		pool_info.maxSets += pool_size.descriptorCount;
	pool_info.poolSizeCount = (uint32_t)1;
	pool_info.pPoolSizes = pool_sizes;

	CR( vkCreateDescriptorPool( device->GetLogicalDevice(), &pool_info, allocator, &m_descriptorPool ) );

	// Setup Platform/Renderer backends
	ImGui_ImplGlfw_InitForVulkan( window, true );
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = m_renderer->GetVulkanInstance();
	init_info.PhysicalDevice = device->GetPhysicalDevice();
	init_info.Device = logicalDevice;
	init_info.QueueFamily = device->GetQueueFamilyIndices().graphicsFamily.value();
	init_info.Queue = device->GetGraphicsQueue();
	init_info.DescriptorPool = m_descriptorPool;
	init_info.MinImageCount = swapchain->GetMinImageCount();
	init_info.ImageCount = swapchain->GetImageCount();
	init_info.Allocator = allocator;
	init_info.RenderPass = VK_NULL_HANDLE;
	init_info.Subpass = 0;
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	init_info.CheckVkResultFn = check_vk_result;
	init_info.UseDynamicRendering = true;

	//dynamic rendering parameters for imgui to use
	auto swapchainFormat = swapchain->GetFormat();
	init_info.PipelineRenderingCreateInfo = {};
	init_info.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
	init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &swapchainFormat;
	init_info.PipelineRenderingCreateInfo.depthAttachmentFormat = m_renderer->GetDepthTexture()->GetFormat();
	init_info.PipelineRenderingCreateInfo.stencilAttachmentFormat = m_renderer->GetDepthTexture()->GetFormat();

	ImGui_ImplVulkan_Init( &init_info );

	state.windowSize.RegisterCallback( [this]( std::pair<uint32_t, uint32_t> size, AppState& appState ) {
		auto [width, height] = size;
		m_graphicsCommandBuffer.SetRenderSize( width, height );
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

	auto [width, height] = state.windowSize.GetValue();
	m_graphicsCommandBuffer.SetRenderSize( width, height );
}


void UIRenderer::BeginFrame()
{
	// Start the Dear ImGui frame
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void UIRenderer::Render( AppState& appState )
{
	SetupUi( appState );
	m_graphicsCommandBuffer.Begin( m_renderer.get() );
	ImGui::Render();

	ImGui_ImplVulkan_RenderDrawData( ImGui::GetDrawData(), m_renderer->GetCurrentGraphicVkCommandBuffer() );
	m_graphicsCommandBuffer.End();
}

void UIRenderer::SetupUi( AppState& appState )
{
	m_menubar.Render( appState );
	if( appState.showUi.GetValue() )
	{
		m_generalWindow.Render( appState, UiConsts::MENU_BAR_HEIGHT, UiConsts::ANIMATION_PLAYER_HEIGHT );
		m_detailWindow.Render( appState, UiConsts::MENU_BAR_HEIGHT, UiConsts::ANIMATION_PLAYER_HEIGHT );
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
			ImGui::SameLine( availableSize.x / 2.0f - 50.0f );

			if( ImGui::Button( "Ok", ImVec2( 100.0, 24 ) ) )
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
			appState.cameraTrigger.ForceSetValue( CameraTrigger::CAMERA_TRIGGER_FOCUS );
		}
	}
	if( ImGui::IsKeyChordPressed( ImGuiMod_Ctrl | ImGuiKey_F12 ) )
	{
		appState.showUi.SetValue( !appState.showUi.GetValue() );
	}
}


// NOLINTEND(cppcoreguidelines-pro-type-vararg)
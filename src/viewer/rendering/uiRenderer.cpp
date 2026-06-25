// Copyright © 2026 CCP ehf.

#include "uiRenderer.h"

#include <algorithm>
#include <cmf/converters.h>
#include <cmf/bufferstreams.h>
#include <cmf/declutils.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <imgui_internal.h>
#include <tinyfiledialogs/tinyfiledialogs.h>

#include "appState.h"
#include "vulkan/vulkanerrors.h"

#include <limits>

// ImGui is using a lot of variadic functions for text formatting, so we disable the cppcoreguidelines-pro-type-vararg lint for this file
// NOLINTBEGIN(cppcoreguidelines-pro-type-vararg)
//
// taken from https://github.com/ocornut/imgui/issues/2644
namespace ImGui
{

// threeway checkbox
bool CheckBoxTristate( const char* label, CheckBoxTriStateValue* v_tristate )
{
	bool ret;
	if( *v_tristate == CheckBoxTriStateValue::MIXED )
	{
		ImGui::PushItemFlag( ImGuiItemFlags_MixedValue, true );
		bool b = false;
		ret = ImGui::Checkbox( label, &b );
		if( ret )
		{
			*v_tristate = CheckBoxTriStateValue::CHECKED;
		}
		ImGui::PopItemFlag();
	}
	else
	{
		bool b = ( *v_tristate != CheckBoxTriStateValue::UNCHECKED );
		ret = ImGui::Checkbox( label, &b );
		if( ret )
		{
			*v_tristate = (CheckBoxTriStateValue)(int)b;
		}
	}
	return ret;
}
};

const float MENU_BAR_HEIGHT = 18.0f;
const float ANIMATION_PLAYER_HEIGHT = 36.0f;
const float BUTTON_SIZE = 18.0f;

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
		RegisterModelCallbacks( appState );
	} );

	auto [width, height] = state.windowSize.GetValue();
	m_graphicsCommandBuffer.SetRenderSize( width, height );
	UpdateUiState( state );
}

void UIRenderer::RegisterModelCallbacks( AppState& appState )
{
	// Register callbacks for model state changes
	appState.modelState.currentAnimation.RegisterCallback( [this]( std::string animationName, AppState& appState ) {
		m_playback.currentTime = 0.0f;
		m_playback.playing = false;

		auto animationOwner = appState.modelState.activeAnimationOwner.GetValue();
		if( !animationName.empty() && animationOwner != nullptr )
		{
			for( const auto& animation : animationOwner->m_cmfData->animations )
			{
				if( cmf::ToStdString( animation.name ) == animationName )
				{
					m_playback.duration = animation.duration;
					return;
				}
			}
		}
		else
		{
			m_playback.duration = 0.0f;
		}
	} );

	appState.modelState.activeAnimationOwner.RegisterCallback( [this]( std::shared_ptr<CmfContent> activeAnimationOwner, AppState& appState ) {
		appState.modelState.currentAnimation.SetValue( "" );
		appState.modelState.currentAnimationTime.SetValue( 0.0f );
		m_playback = Playback{};
	} );
}

const char* UIRenderer::FileOpenDialog()
{
	char const* filter[1] = { "*.cmf" };
	return tinyfd_openFileDialog(
		"Open CMF File",
		NULL,
		1,
		filter,
		"CMF Files",
		0 );
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
	SetupMenubar( appState );
	SetupUi( appState );
	m_graphicsCommandBuffer.Begin( m_renderer.get() );
	ImGui::Render();

	ImGui_ImplVulkan_RenderDrawData( ImGui::GetDrawData(), m_renderer->GetCurrentGraphicVkCommandBuffer() );
	m_graphicsCommandBuffer.End();
}

void UIRenderer::SetupUi( AppState& appState )
{
	UpdateUiState( appState );
	if( m_showMainUI )
	{
		CMFInfoWindow( appState );
		m_detailWindow.Render( appState, MENU_BAR_HEIGHT, ANIMATION_PLAYER_HEIGHT );

		SetupPlaybackControls( appState );
	}
	SetupPopupWindows( appState );
}

void UIRenderer::CMFInfoWindow( AppState& appState )
{
	bool open = true;
	float width = (float)appState.windowSize.GetValue().first;
	float height = (float)appState.windowSize.GetValue().second;

	float ySize = std::max( 1.0f, height - MENU_BAR_HEIGHT - ANIMATION_PLAYER_HEIGHT + 1 ); // +1 so we get an  overlap of the borders

	ImGui::SetNextWindowPos( ImVec2( 0, 18 ), ImGuiCond_Always );
	ImGui::SetNextWindowSizeConstraints( ImVec2( 0, ySize ), ImVec2( width, ySize ) );
	ImGui::SetNextWindowSize( ImVec2( width / 4.0f, ySize ), ImGuiCond_FirstUseEver );
	if( ImGui::Begin( "CMF Info", &open, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings ) )
	{
		SetupGeneralView( appState );

		SetupMeshListView( m_uiState.modelStates, appState );

		SetupSkeletonOwners( m_uiState.skeletonOwners, appState );
	}
	ImGui::End();
}

void UIRenderer::SetupGeneralView( AppState& appState )
{
	ImGui::SeparatorText( "General" );
	if( ImGui::BeginTable( "##table", 2 ) )
	{
		ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_WidthFixed );
		ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_WidthStretch );
		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		ImGui::Text( "Path" );
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x );
		ImGui::InputText( "##label", m_uiState.filePath.data(), m_uiState.filePath.size(), ImGuiInputTextFlags_ReadOnly );
		ImGui::SetItemTooltip( "%s", m_uiState.filePath.data() );

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text( "Lod" );
		ImGui::TableNextColumn();
		SetupCombo( "##lod", m_uiState.modelStates.lod, appState.modelState.selectedLod );
		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		ImGui::Text( "Vertices" );
		ImGui::TableNextColumn();
		ImGui::Text( "%u", m_uiState.modelStates.vertexCount );
		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		ImGui::Text( "Indices" );
		ImGui::TableNextColumn();
		ImGui::Text( "%u", m_uiState.modelStates.indexCount );
		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		ImGui::Text( "Meshes" );
		ImGui::TableNextColumn();
		ImGui::Text( "%zu", m_uiState.modelStates.meshes.size() );
		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		ImGui::Text( "Polygon Mode" );
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x );
		SetupCombo( "##polygonmode", m_uiState.polygonModeComboBox, appState.modelState.polygonMode );
		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		ImGui::Text( "Visualization" );
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x );
		SetupCombo( "##visualiationMode", m_uiState.visualizationShaderComboBox, appState.modelState.activeShader );
		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		ImGui::Text( "Display" );
		ImGui::TableNextColumn();
		size_t checkedCount = std::count_if( appState.modelState.meshes.begin(), appState.modelState.meshes.end(), []( const State<MeshState>& state ) {
			return state.GetValue().display.GetValue();
		} );
		ImGui::CheckBoxTriStateValue checked = ( checkedCount == 0 ) ? ImGui::CheckBoxTriStateValue::UNCHECKED : ( checkedCount == appState.modelState.meshes.size() ? ImGui::CheckBoxTriStateValue::CHECKED : ImGui::CheckBoxTriStateValue::MIXED );
		OnChange( ImGui::CheckBoxTristate( "##displaycheckbox", &checked ), [&appState, &checked]() {
			std::for_each( appState.modelState.meshes.begin(), appState.modelState.meshes.end(), [checked]( State<MeshState>& state ) {
				state.GetValue().display.SetValue( checked == ImGui::CheckBoxTriStateValue::CHECKED );
			} );
		} );
		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		ImGui::Text( "Bounding Box" );
		ImGui::TableNextColumn();
		bool boundingBox = m_uiState.modelStates.boundingBox;
		OnChange( ImGui::Checkbox( "##boundingboxcheckbox", &boundingBox ), [&appState, &boundingBox]() {
			appState.modelState.modelBoundingBox.SetValue( boundingBox );
		} );
		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		ImGui::Text( "Wireframe Overlay" );
		ImGui::TableNextColumn();
		checkedCount = std::count_if( appState.modelState.meshes.begin(), appState.modelState.meshes.end(), []( const State<MeshState>& state ) {
			return state.GetValue().wireframeOverlay.GetValue();
		} );
		checked = ( checkedCount == 0 ) ? ImGui::CheckBoxTriStateValue::UNCHECKED : ( checkedCount == appState.modelState.meshes.size() ? ImGui::CheckBoxTriStateValue::CHECKED : ImGui::CheckBoxTriStateValue::MIXED );
		OnChange( ImGui::CheckBoxTristate( "##wireframecheckbox", &checked ), [&appState, &checked]() {
			std::for_each( appState.modelState.meshes.begin(), appState.modelState.meshes.end(), [checked]( State<MeshState>& state ) {
				state.GetValue().wireframeOverlay.SetValue( checked == ImGui::CheckBoxTriStateValue::CHECKED );
			} );
		} );
		ImGui::TableNextRow();

		bool hasAudioOcclusionMeshes = std::any_of( m_uiState.modelStates.meshes.begin(), m_uiState.modelStates.meshes.end(), []( const MeshUiState& state ) {
			return state.hasAudioOcclusionMesh;
		} );
		ImGui::BeginDisabled( !hasAudioOcclusionMeshes );
		ImGui::TableNextColumn();
		ImGui::Text( "Audio Occlusion Mesh" );
		ImGui::TableNextColumn();
		checkedCount = std::count_if( appState.modelState.meshes.begin(), appState.modelState.meshes.end(), []( const State<MeshState>& state ) {
			return state.GetValue().audioOcclusionMesh.GetValue();
		} );
		checked = ( checkedCount == 0 ) ? ImGui::CheckBoxTriStateValue::UNCHECKED : ( checkedCount == appState.modelState.meshes.size() ? ImGui::CheckBoxTriStateValue::CHECKED : ImGui::CheckBoxTriStateValue::MIXED );
		OnChange( ImGui::CheckBoxTristate( "##audioocclusioncheckbox", &checked ), [&appState, &checked]() {
			std::for_each( appState.modelState.meshes.begin(), appState.modelState.meshes.end(), [checked]( State<MeshState>& state ) {
				state.GetValue().audioOcclusionMesh.SetValue( checked == ImGui::CheckBoxTriStateValue::CHECKED );
			} );
		} );
		ImGui::TableNextRow();
		ImGui::EndDisabled();

		SetupModelAxisRows( appState );

		ImGui::EndTable();
	}
}

void UIRenderer::SetupMeshListView( const ModelUiState& modelState, AppState& appState )
{
	bool open = true;

	if( !modelState.meshes.empty() )
	{
		std::string header = "Meshes (" + std::to_string( modelState.meshes.size() ) + ")";
		if( ImGui::CollapsingHeader( header.c_str(), &open, ImGuiTreeNodeFlags_DefaultOpen ) )
		{
			for( const auto& mesh : modelState.meshes )
			{
				SetupMeshView( mesh, appState );
			}
		}
	}
}

void UIRenderer::SetupModelAxisRows( AppState& appState )
{
	std::vector<StateCollection<std::pair<uint32_t, bool>>> allNormalStates;
	std::vector<StateCollection<std::pair<uint32_t, bool>>> allTangentStates;
	std::vector<StateCollection<std::pair<uint32_t, bool>>> allBinormalStates;

	for( const auto& meshState : appState.modelState.meshes )
	{
		allNormalStates.push_back( meshState.GetValue().showVertexNormals );
		allTangentStates.push_back( meshState.GetValue().showVertexTangents );
		allBinormalStates.push_back( meshState.GetValue().showVertexBinormals );
	}

	auto normalCheckboxes = GetAxisTriCheckboxStates( allNormalStates );
	auto tangentCheckboxes = GetAxisTriCheckboxStates( allTangentStates );
	auto binormalCheckboxes = GetAxisTriCheckboxStates( allBinormalStates );

	SetupModelAxisRow( normalCheckboxes, std::string( "Normals" ), [&appState]( bool checked, uint32_t index ) {
		for( auto& meshState : appState.modelState.meshes )
		{
			auto& vertexNormalStates = meshState.GetValue().showVertexNormals;
			auto foundState = std::find_if( vertexNormalStates.begin(), vertexNormalStates.end(), [index]( const State<std::pair<uint32_t, bool>>& state ) {
				return state.GetValue().first == index;
			} );
			if( foundState != vertexNormalStates.end() )
			{
				foundState->SetValue( { index, checked } );
			}
		}
	} );

	SetupModelAxisRow( tangentCheckboxes, std::string( "Tangents" ), [&appState]( bool checked, uint32_t index ) {
		for( auto& meshState : appState.modelState.meshes )
		{
			auto& vertexTangentStates = meshState.GetValue().showVertexTangents;
			auto foundState = std::find_if( vertexTangentStates.begin(), vertexTangentStates.end(), [index]( const State<std::pair<uint32_t, bool>>& state ) {
				return state.GetValue().first == index;
			} );
			if( foundState != vertexTangentStates.end() )
			{
				foundState->SetValue( { index, checked } );
			}
		}
	} );

	SetupModelAxisRow( binormalCheckboxes, std::string( "Bitangents" ), [&appState]( bool checked, uint32_t index ) {
		for( auto& meshState : appState.modelState.meshes )
		{
			auto& vertexBinormalStates = meshState.GetValue().showVertexBinormals;
			auto foundState = std::find_if( vertexBinormalStates.begin(), vertexBinormalStates.end(), [index]( const State<std::pair<uint32_t, bool>>& state ) {
				return state.GetValue().first == index;
			} );
			if( foundState != vertexBinormalStates.end() )
			{
				foundState->SetValue( { index, checked } );
			}
		}
	} );
}

std::vector<std::pair<uint32_t, ImGui::CheckBoxTriStateValue>> UIRenderer::GetAxisTriCheckboxStates( const std::vector<StateCollection<std::pair<uint32_t, bool>>>& axisStates )
{
	std::vector<std::pair<uint32_t, ImGui::CheckBoxTriStateValue>> combinedStates;

	for( const auto& meshState : axisStates )
	{
		for( const auto& axis : meshState )
		{
			auto [index, checked] = axis.GetValue();

			auto foundState = std::find_if( combinedStates.begin(), combinedStates.end(), [idx = index]( const std::pair<uint32_t, ImGui::CheckBoxTriStateValue>& pair ) {
				return pair.first == idx;
			} );

			if( foundState == combinedStates.end() )
			{
				combinedStates.push_back( { index, checked ? ImGui::CheckBoxTriStateValue::CHECKED : ImGui::CheckBoxTriStateValue::UNCHECKED } );
			}
			else
			{
				if( foundState->second != ImGui::CheckBoxTriStateValue::MIXED )
				{
					if( ( foundState->second == ImGui::CheckBoxTriStateValue::CHECKED && !checked ) || ( foundState->second == ImGui::CheckBoxTriStateValue::UNCHECKED && checked ) )
					{
						foundState->second = ImGui::CheckBoxTriStateValue::MIXED;
					}
				}
			}
		}
	}

	return combinedStates;
}

void UIRenderer::SetupVertexAxisRows( MeshState& meshAppState )
{
	auto usageIndexChecker = []( const State<std::pair<uint32_t, bool>>& pair1, const State<std::pair<uint32_t, bool>>& pair2 ) {
		return pair1.GetValue().first < pair2.GetValue().first;
	};

	auto maxNormalIndexState = std::max_element( meshAppState.showVertexNormals.begin(), meshAppState.showVertexNormals.end(), usageIndexChecker );
	auto maxTangentIndexState = std::max_element( meshAppState.showVertexTangents.begin(), meshAppState.showVertexTangents.end(), usageIndexChecker );
	auto maxBitangentIndexState = std::max_element( meshAppState.showVertexBinormals.begin(), meshAppState.showVertexBinormals.end(), usageIndexChecker );

	int32_t maxNormalIndex = maxNormalIndexState != meshAppState.showVertexNormals.end() ? maxNormalIndexState->GetValue().first : -1;
	int32_t maxTangentIndex = maxTangentIndexState != meshAppState.showVertexTangents.end() ? maxTangentIndexState->GetValue().first : -1;
	int32_t maxBitangentIndex = maxBitangentIndexState != meshAppState.showVertexBinormals.end() ? maxBitangentIndexState->GetValue().first : -1;

	SetupVertexAxisRow( meshAppState.showVertexNormals, "Normals", maxNormalIndex );
	SetupVertexAxisRow( meshAppState.showVertexTangents, "Tangents", maxTangentIndex );
	SetupVertexAxisRow( meshAppState.showVertexBinormals, "Bitangents", maxBitangentIndex );
}

void UIRenderer::SetupVertexAxisRow( StateCollection<std::pair<uint32_t, bool>>& vertexAxisStates, const char* label, int maxUsageIndex )
{
	if( vertexAxisStates.empty() )
	{
		ImGui::BeginDisabled();
	}
	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::TextUnformatted( label );
	ImGui::TableNextColumn();

	if( maxUsageIndex >= 0 )
	{
		for( int32_t i = 0; i <= maxUsageIndex; ++i )
		{
			// find the states that have the index of the column (f.ex there is nothing stopping a mesh having one color attribute that is using usageindex at 16)
			auto foundElement = std::find_if( vertexAxisStates.begin(), vertexAxisStates.end(), [i]( const State<std::pair<uint32_t, bool>>& state ) {
				return state.GetValue().first == i;
			} );

			if( foundElement != vertexAxisStates.end() )
			{
				bool changed = ImGui::Checkbox( ( std::string( "##" ) + label + std::to_string( i ) ).c_str(), &foundElement->GetValue().second );

				OnChange( changed, [foundElement]() {
					foundElement->SetValue( { foundElement->GetValue().first, foundElement->GetValue().second } );
				} );

				ImGui::SetItemTooltip( "Toggles debug visualization for %s with usage index %d", label, i );
				ImGui::SameLine();
			}
			else
			{
				ImGui::BeginDisabled( true );
				bool value = false;
				ImGui::Checkbox( ( std::string( "##" ) + label + std::to_string( i ) ).c_str(), &value );
				ImGui::SetItemTooltip( "Mesh doesn't have %s with usage index %d.", label, i );
				ImGui::SameLine();
				ImGui::EndDisabled();
			}
		}
		ImGui::NewLine();
	}
	if( vertexAxisStates.empty() )
	{
		ImGui::EndDisabled();
	}
}

void UIRenderer::SetupMeshView( const MeshUiState& mesh, AppState& appState )
{
	if( ImGui::TreeNode( mesh.name.c_str() ) )
	{
		if( ImGui::BeginTable( "##table", 2 ) )
		{
			auto& meshAppState = appState.modelState.meshes[mesh.meshIndex].GetValue();
			ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_WidthFixed );
			ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_WidthStretch );
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text( "Vertex Count" );
			ImGui::TableNextColumn();
			ImGui::Text( "%u", mesh.vertexCount );

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text( "Index Count" );
			ImGui::TableNextColumn();
			ImGui::Text( "%u", mesh.indexCount );

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text( "Current Lod" );
			ImGui::TableNextColumn();
			ImGui::Text( "%u/%u", mesh.lod, mesh.maxLodIndex );

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text( "Screen Size" );
			ImGui::SetItemTooltip( "The approximate screen size based on the generated bounding sphere with radius %.3f (if it is infinite then the camera is inside of the sphere)", mesh.boundingSphereRadius );
			ImGui::TableNextColumn();
			if( mesh.screenSize == std::numeric_limits<float>::max() )
			{
				ImGui::Text( "infinite" );
			}
			else
			{
				ImGui::Text( "%upx", uint32_t( mesh.screenSize ) );
			}

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text( "Display" );
			ImGui::SetItemTooltip( "Toggles the \"%s\" mesh", mesh.name.c_str() );

			ImGui::TableNextColumn();
			bool display = mesh.display;
			OnChange( ImGui::Checkbox( "##displaycheckbox", &display ), [&meshAppState, &display]() {
				meshAppState.display.SetValue( display );
			} );

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text( "Bounding Box" );
			ImGui::SetItemTooltip( "Toggles the bounding box for \"%s\" mesh", mesh.name.c_str() );

			ImGui::TableNextColumn();
			bool boundingBox = mesh.boundingBox;
			OnChange( ImGui::Checkbox( "##boundingboxcheckbox", &boundingBox ), [&meshAppState, &boundingBox]() {
				meshAppState.renderBoundingBox.SetValue( boundingBox );
			} );
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text( "Wireframe Overlay" );
			ImGui::SetItemTooltip( "Toggles the wireframe overlay for the \"%s\" mesh", mesh.name.c_str() );

			ImGui::TableNextColumn();
			bool wireframeOverlay = mesh.wireframeOverlay;
			OnChange( ImGui::Checkbox( "##wireframeoverlaycheckbox", &wireframeOverlay ), [&meshAppState, &wireframeOverlay]() {
				meshAppState.wireframeOverlay.SetValue( wireframeOverlay );
			} );
			ImGui::TableNextRow();

			ImGui::BeginDisabled( !mesh.hasAudioOcclusionMesh );
			ImGui::TableNextColumn();
			ImGui::Text( "Audio Occlusion Mesh" );
			ImGui::SetItemTooltip( "Toggles the audio occlusion mesh rendering for the \"%s\" mesh", mesh.name.c_str() );
			ImGui::TableNextColumn();
			bool audioOcclusion = mesh.audioOcclusionMesh;
			OnChange( ImGui::Checkbox( "##audioocclusionmeshcheckbox", &audioOcclusion ), [&meshAppState, &audioOcclusion]() {
				meshAppState.audioOcclusionMesh.SetValue( audioOcclusion );
			} );
			ImGui::EndDisabled();

			SetupVertexAxisRows( meshAppState );

			ImGui::EndTable();

			std::string header = "Morph Targets (" + std::to_string( mesh.morphTargets.size() ) + ")";
			if( ImGui::CollapsingHeader( header.c_str() ) )
			{
				for( const auto& morphTarget : mesh.morphTargets )
				{
					SetupMorphTarget( morphTarget, mesh.meshIndex, appState );
				}
			}
		}
		ImGui::TreePop();
	}
}

void UIRenderer::SetupMorphTarget( const MorphTargetUiState& morphTarget, size_t meshIndex, AppState& appState )
{
	ImGui::SeparatorText( morphTarget.name.c_str() );

	if( ImGui::BeginTable( "##table", 3 ) )
	{
		ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_WidthFixed );
		ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_WidthStretch );
		ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_WidthFixed, BUTTON_SIZE );
		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		ImGui::Text( "Weight" );
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x );
		float weight = morphTarget.weight;
		OnChange( ImGui::SliderFloat( "##slider", &weight, 0.0f, 1.0f, "%.6f" ), [&appState, &morphTarget, &weight, meshIndex]() {
			auto& morphState = appState.modelState.meshes[meshIndex].GetValue().morphs[morphTarget.index];
			morphState.SetValue( { weight, morphState.GetValue().second } );
		} );
		ImGui::TableNextColumn();
		bool enabled = morphTarget.enabled;

		OnChange( ImGui::Checkbox( "##checkbox", &enabled ), [&appState, &morphTarget, &enabled, meshIndex]() {
			auto& morphState = appState.modelState.meshes[meshIndex].GetValue().morphs[morphTarget.index];
			morphState.SetValue( { morphState.GetValue().first, enabled } );
		} );
		ImGui::SetItemTooltip( "Toggles the \"%s\" morph target", morphTarget.name.c_str() );
		ImGui::EndTable();
	}
}

void UIRenderer::SetupSkeletonOwners( const std::vector<SkeletonOwnerUiState>& skeletonOwners, AppState& appState )
{
	// static meshes
	if( skeletonOwners.empty() )
	{
		std::string header = "Skeletons ( 0 )";
		if( ImGui::CollapsingHeader( header.c_str(), ImGuiTreeNodeFlags_None ) )
		{
			ImGui::BeginDisabled();
			ImGui::Button( "+", ImVec2( BUTTON_SIZE, BUTTON_SIZE ) );
			ImGui::SetItemTooltip( "Adds an animation override - Disabled since model doesn't have a skeleton" );
			ImGui::EndDisabled();
		}
		return;
	}

	// The first skeleton owner is the model, the rest are overrides
	std::string header = "Skeletons ( " + std::to_string( skeletonOwners.size() ) + " )";
	if( ImGui::CollapsingHeader( header.c_str(), ImGuiTreeNodeFlags_DefaultOpen ) )
	{
		// debug options show bones, joints and joint axis
		ImGui::BeginTable( "##SkeletonDebug", 2 );
		ImGui::TableNextRow();

		ImGui::TableNextColumn();

		ImGui::Text( "Skeleton Joint" );
		ImGui::TableNextColumn();
		OnChange( ImGui::Checkbox( "##skeletonjoint", &m_uiState.jointDebug ), [&appState, this]() {
			appState.modelState.jointDebug.SetValue( m_uiState.jointDebug );
		} );
		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		ImGui::Text( "Skeleton Joint Axis" );
		ImGui::TableNextColumn();
		OnChange( ImGui::Checkbox( "##skeletonjointaxis", &m_uiState.jointAxisDebug ), [&appState, this]() {
			appState.modelState.jointAxisDebug.SetValue( m_uiState.jointAxisDebug );
		} );
		ImGui::TableNextRow();
		ImGui::TableNextColumn();

		ImGui::Text( "Skeleton Bones" );
		ImGui::TableNextColumn();
		OnChange( ImGui::Checkbox( "##skeletonbones", &m_uiState.boneDebug ), [&appState, this]() {
			appState.modelState.boneDebug.SetValue( m_uiState.boneDebug );
		} );
		ImGui::TableNextRow();
		ImGui::EndTable();

		ImGui::SeparatorText( "Animation Owners" );
		OnChange( ImGui::Button( "+", ImVec2( ImGui::GetContentRegionAvail().x, BUTTON_SIZE ) ), [this, &appState]() {
			auto* path = this->FileOpenDialog();
			if( path != nullptr )
			{
				auto data = CmfContentLoader::LoadContentFromFile( path );
				if( data )
				{
					appState.modelState.animationOverrides.AddState( data );
				}
			}
		} );

		ImGui::SetItemTooltip( "Adds an animation owner from a cmf file" );

		for( int32_t index = 0; index < skeletonOwners.size(); ++index )
		{
			ImGui::PushID( index );

			auto skeletonOwner = skeletonOwners[index];

			if( ImGui::RadioButton( skeletonOwner.shortSource.c_str(), appState.modelState.activeAnimationOwner.GetValue() == skeletonOwner.cmfContent ) )
			{
				appState.modelState.activeAnimationOwner.SetValue( skeletonOwner.cmfContent );
			}

			if( ImGui::BeginItemTooltip() )
			{
				if( index == 0 )
				{
					ImGui::Text( "Model skeleton and animations" );
				}
				else
				{
					ImGui::Text( "Animation owner from %s", skeletonOwner.source.c_str() );
				}
				ImGui::Text( "Skeletons" );
				for( const auto& skeleton : skeletonOwner.skeletons )
				{
					ImGui::BulletText( "%s has %d bones", skeleton.name.c_str(), skeleton.bonesCount );
				}
				ImGui::EndTooltip();
			}

			if( index != 0 )
			{
				ImGui::SameLine( ImGui::GetContentRegionAvail().x - BUTTON_SIZE );
				OnChange( ImGui::Button( "-", ImVec2( BUTTON_SIZE, BUTTON_SIZE ) ), [&appState, skeletonOwner, index]() {
					appState.modelState.animationOverrides.RemoveAt( index - 1 );

					if( appState.modelState.activeAnimationOwner.GetValue() == skeletonOwner.cmfContent )
					{
						appState.modelState.activeAnimationOwner.SetValue( appState.cmfContent.GetValue() );
					}
				} );
				ImGui::SetItemTooltip( "Removes %s ", skeletonOwner.source.c_str() );
			}

			ImGui::PopID();
		}
	}
}

void UIRenderer::SetupSkeletons( const std::vector<SkeletonUiState>& skeletonStates, AppState& appState )
{
	for( const auto& skeleton : skeletonStates )
	{
		if( ImGui::TreeNode( skeleton.name.c_str() ) )
		{
			if( ImGui::BeginTable( "##table", 3 ) )
			{
				ImGui::TableNextRow();

				ImGui::TableNextColumn();

				ImGui::Text( "Bone Count" );
				ImGui::TableNextColumn();
				ImGui::Text( "%u", skeleton.bonesCount );
			}
			ImGui::TreePop();
		}
	}
}

void UIRenderer::SetupMenubar( AppState& appState )
{
	if( ImGui::BeginMainMenuBar() )
	{
		if( ImGui::BeginMenu( "File" ) )
		{
			if( ImGui::MenuItem( "Open", "Ctrl+O" ) )
			{
				auto filePath = FileOpenDialog();
				if( filePath != nullptr )
				{
					appState.ResetModelState();
					appState.cmfContent.ForceSetValue( CmfContentLoader::LoadContentFromFile( filePath ) );
					appState.cmfPath.SetValue( std::string( filePath ) );
				}
			}
			if( appState.cmfContent.GetValue() == nullptr )
			{
				ImGui::BeginDisabled();
			}
			if( ImGui::MenuItem( "Load Animation Override" ) )
			{
				const auto* filePath = FileOpenDialog();
				if( filePath != nullptr )
				{
					auto data = CmfContentLoader::LoadContentFromFile( filePath );
					if( data )
					{
						appState.modelState.animationOverrides.AddState( data );
					}
				}
			}
			if( appState.cmfContent.GetValue() == nullptr )
			{
				ImGui::EndDisabled();
			}

			ImGui::Separator();
			if( ImGui::MenuItem( "Exit" ) )
			{
				appState.exitRequested.SetValue( true );
			}
			ImGui::EndMenu();
		}
		if( ImGui::BeginMenu( "View" ) )
		{
			if( ImGui::BeginMenu( "Camera" ) )
			{
				if( ImGui::MenuItem( "Focus", "Ctrl+F" ) )
				{
					appState.cameraTrigger.ForceSetValue( CameraTrigger::CAMERA_TRIGGER_FOCUS );
				}
				if( ImGui::MenuItem( "Look Right (+X)" ) )
				{
					appState.cameraTrigger.ForceSetValue( CameraTrigger::CAMERA_TRIGGER_LOOK_RIGHT );
				}
				if( ImGui::MenuItem( "Look Left (-X)" ) )
				{
					appState.cameraTrigger.ForceSetValue( CameraTrigger::CAMERA_TRIGGER_LOOK_LEFT );
				}
				if( ImGui::MenuItem( "Look Up (+Y)" ) )
				{
					appState.cameraTrigger.ForceSetValue( CameraTrigger::CAMERA_TRIGGER_LOOK_UP );
				}
				if( ImGui::MenuItem( "Look Down (-Y)" ) )
				{
					appState.cameraTrigger.ForceSetValue( CameraTrigger::CAMERA_TRIGGER_LOOK_DOWN );
				}
				if( ImGui::MenuItem( "Look Front (-Z)" ) )
				{
					appState.cameraTrigger.ForceSetValue( CameraTrigger::CAMERA_TRIGGER_LOOK_FRONT );
				}
				if( ImGui::MenuItem( "Look Back (+Z)" ) )
				{
					appState.cameraTrigger.ForceSetValue( CameraTrigger::CAMERA_TRIGGER_LOOK_BACK );
				}

				ImGui::EndMenu();
			}
			ImGui::Separator();
			const char* toggleUiLabel = m_showMainUI ? "Hide UI" : "Show UI";
			if( ImGui::MenuItem( toggleUiLabel, "Ctrl+F12" ) )
			{
				ToggleUiVisibility();
			}
			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}
}

void UIRenderer::ToggleUiVisibility()
{
	m_showMainUI = !m_showMainUI;
}

const char* UIRenderer::GetPlaybackButtonLabel() const
{
	if( m_playback.playing )
	{
		return "Pause";
	}
	return "Play";
}

void UIRenderer::HandlePlaybackButtonPressed()
{
	m_playback.playing = !m_playback.playing;
	if( m_playback.currentTime == m_playback.duration )
	{
		m_playback.currentTime = 0.0f;
	}
}

void UIRenderer::StepAnimation( float amount, AppState& appState )
{
	m_playback.playing = false;
	// step a tenth of a sec
	m_playback.currentTime += amount;
	m_playback.currentTime = std::max( 0.0f, std::min( m_playback.currentTime, m_playback.duration ) );
	appState.modelState.currentAnimationTime.SetValue( m_playback.currentTime );
}

void UIRenderer::SetupPlaybackControls( AppState& appState )
{
	float width = (float)appState.windowSize.GetValue().first;
	float height = (float)appState.windowSize.GetValue().second;

	// animation player
	ImGui::SetNextWindowPos( ImVec2( 0, height - ANIMATION_PLAYER_HEIGHT ), ImGuiCond_Always );
	ImGui::SetNextWindowSize( ImVec2( width, ANIMATION_PLAYER_HEIGHT ), ImGuiCond_Always );
	bool open = true;
	if( ImGui::Begin( "Animation", &open, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize ) )
	{
		ImGui::PushItemWidth( 120.0f );
		// selection box to the right
		SetupCombo( "##animation", m_playback.animationComboBox, appState.modelState.currentAnimation );
		ImGui::SameLine();

		bool disabled = m_playback.animationComboBox.selectedItemValue == "";
		if( disabled )
		{
			ImGui::BeginDisabled();
		}
		OnChange( ImGui::Button( "<", ImVec2( 18.0f, 18.0f ) ), [this, &appState]() {
			StepAnimation( -0.1f, appState );
		} );

		ImGui::SameLine();
		OnChange( ImGui::Button( GetPlaybackButtonLabel(), ImVec2( 64.0f, 18.0f ) ), [this]() { HandlePlaybackButtonPressed(); } );

		ImGui::SameLine();
		OnChange( ImGui::Button( ">", ImVec2( 18.0f, 18.0f ) ), [this, &appState]() {
			StepAnimation( 0.1f, appState );
		} );

		ImGui::SameLine();
		ImGui::PushItemWidth( 30.0f );
		ImGui::Checkbox( "##repeatanimation", &m_playback.repeat );
		ImGui::SetItemTooltip( "Repeat the animation" );

		ImGui::SameLine();
		float availableWidth = ImGui::GetContentRegionAvail().x;
		ImGui::PushItemWidth( availableWidth );
		OnChange( ImGui::SliderFloat( "##playbackSlider", &m_playback.currentTime, 0.0f, m_playback.duration ), [&appState, this]() {
			appState.modelState.currentAnimationTime.SetValue( m_playback.currentTime );
		} );
		ImGui::SetItemTooltip( "%.3fs / %.3fs", m_playback.currentTime, m_playback.duration );

		if( disabled )
		{
			ImGui::EndDisabled();
		}
		ImGui::SameLine();
	}
	ImGui::End();
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
	UpdatePlayback( appstate );
	UpdateInputs( appstate );
}

void UIRenderer::UpdatePlayback( AppState& appState )
{
	if( m_playback.playing )
	{
		m_playback.currentTime += ImGui::GetIO().DeltaTime;
		appState.modelState.currentAnimationTime.SetValue( m_playback.currentTime );
		if( m_playback.currentTime >= m_playback.duration )
		{
			if( m_playback.repeat )
			{
				m_playback.currentTime = 0.0f;
			}
			else
			{
				m_playback.currentTime = m_playback.duration;
				m_playback.playing = false;
			}
		}
	}
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
		auto filePath = FileOpenDialog();
		if( filePath != nullptr )
		{
			appState.ResetModelState();
			appState.cmfContent.ForceSetValue( CmfContentLoader::LoadContentFromFile( filePath ) );
			appState.cmfPath.SetValue( std::string( filePath ) );
		}
	}
	if( ImGui::IsKeyChordPressed( ImGuiMod_Ctrl | ImGuiKey_F ) )
	{
		// focus camera on model
		appState.cameraTrigger.ForceSetValue( CameraTrigger::CAMERA_TRIGGER_FOCUS );
	}
	if( ImGui::IsKeyChordPressed( ImGuiMod_Ctrl | ImGuiKey_F12 ) )
	{
		ToggleUiVisibility();
	}
}

void UIRenderer::UpdateUiState( AppState& appState )
{
	if( m_loadStatus == LoadStatus::SUCCESSFUL )
	{
		// reset all appState items related to cmf
		appState.modelState.selectedLod.SetValue( -1 );
		appState.modelState.currentAnimation.SetValue( "" );
		appState.modelState.currentAnimationTime.SetValue( 0.0f );
		m_playback = Playback{};
	}

	m_uiState = UiState{};
	auto cmfContent = appState.cmfContent.GetValue();
	if( cmfContent != nullptr )
	{
		m_uiState.filePath = appState.cmfPath.GetValue();
		// polygon mode selection
		m_uiState.polygonModeComboBox.items = {
			{ "Fill", VK_POLYGON_MODE_FILL },
			{ "Line", VK_POLYGON_MODE_LINE },
			{ "Point", VK_POLYGON_MODE_POINT }
		};
		m_uiState.polygonModeComboBox.SetSelectedItemByValue( appState.modelState.polygonMode.GetValue() );

		size_t meshIndex = 0;
		size_t morphIndex = 0;
		size_t maxLod = 0;
		uint32_t skeletonIndex = 0;


		m_uiState.jointDebug = appState.modelState.jointDebug.GetValue();
		m_uiState.jointAxisDebug = appState.modelState.jointAxisDebug.GetValue();
		m_uiState.boneDebug = appState.modelState.boneDebug.GetValue();

		auto addSkeletonOwner = [this]( const std::string& source, const std::string& shortNameOverride, std::shared_ptr<CmfContent> data ) {
			SkeletonOwnerUiState skeletonOwnerState{};
			skeletonOwnerState.source = source;
			skeletonOwnerState.cmfContent = data;
			if( shortNameOverride.empty() )
			{
				skeletonOwnerState.shortSource = source;
				skeletonOwnerState.shortSource = skeletonOwnerState.shortSource.substr( skeletonOwnerState.shortSource.find_last_of( "/\\" ) + 1 );
				skeletonOwnerState.shortSource = skeletonOwnerState.shortSource.substr( skeletonOwnerState.shortSource.find_last_of( "//" ) + 1 );
			}
			else
			{
				skeletonOwnerState.shortSource = shortNameOverride;
			}
			for( const auto& skeleton : data->m_cmfData->skeletons )
			{
				SkeletonUiState skeletonState{};
				skeletonState.name = cmf::ToStdString( skeleton.name );
				skeletonState.bonesCount = static_cast<uint32_t>( skeleton.bones.size() );
				skeletonOwnerState.skeletons.push_back( skeletonState );
			}
			m_uiState.skeletonOwners.push_back( skeletonOwnerState );
		};

		// Add skeleton owners
		addSkeletonOwner( cmfContent->m_filePath, "Model Skeleton", cmfContent );
		for( const auto& animationOverride : appState.modelState.animationOverrides )
		{
			addSkeletonOwner( animationOverride.GetValue()->m_filePath, "", animationOverride.GetValue() );
		}

		for( const auto& mesh : cmfContent->m_cmfData->meshes )
		{
			if( meshIndex >= appState.modelState.meshes.size() )
			{
				break;
			}
			const auto& meshAppState = appState.modelState.meshes[meshIndex].GetValue();
			MeshUiState meshState{};
			meshState.meshIndex = meshIndex;
			meshState.name = cmf::ToStdString( mesh.name );
			meshState.lod = meshAppState.activeLod.GetValue();
			meshState.maxLodIndex = static_cast<uint32_t>( mesh.lods.size() ) - 1;
			meshState.boundingSphereRadius = CcpMath::Sphere( mesh.bounds ).radius;
			meshState.screenSize = meshAppState.meshScreenSize.GetValue();
			meshState.display = meshAppState.display.GetValue();
			meshState.wireframeOverlay = meshAppState.wireframeOverlay.GetValue();
			meshState.audioOcclusionMesh = meshAppState.audioOcclusionMesh.GetValue();
			meshState.hasAudioOcclusionMesh = !mesh.audioOcclusionMesh.vertices.empty() && !mesh.audioOcclusionMesh.indices.empty();
			meshState.boundingBox = meshAppState.renderBoundingBox.GetValue();

			maxLod = std::max( maxLod, mesh.lods.size() );
			meshState.vertexCount = 0;
			meshState.indexCount = 0;
			if( meshState.lod < mesh.lods.size() )
			{
				const auto& lod = mesh.lods[meshState.lod];
				if( lod.vb.stride > 0 )
				{
					meshState.vertexCount = cmf::GetStreamElementCount( lod.vb );
				}
				if( lod.ib.stride > 0 )
				{
					meshState.indexCount = cmf::GetStreamElementCount( lod.ib );
				}
			}

			m_uiState.modelStates.vertexCount += meshState.vertexCount;
			m_uiState.modelStates.indexCount += meshState.indexCount;

			for( size_t i = 0; i < mesh.morphTargets.targets.size(); ++i )
			{
				if( i >= meshAppState.morphs.size() )
				{
					break;
				}
				const auto& morphTarget = mesh.morphTargets.targets[i];
				const auto& morphPair = meshAppState.morphs[i].GetValue();
				MorphTargetUiState morphTargetState{};
				morphTargetState.name = cmf::ToStdString( morphTarget.name );
				morphTargetState.weight = morphPair.first;
				morphTargetState.enabled = morphPair.second;
				morphTargetState.index = i;
				meshState.morphTargets.push_back( morphTargetState );
			}

			m_uiState.modelStates.meshes.push_back( meshState );
			meshIndex++;
		}

		// visualization shader selection
		for( const auto& shaderSetupInfo : appState.modelState.availableShaders.GetValue() )
		{
			std::string shaderName = shaderSetupInfo.first;
			if( !shaderSetupInfo.second.shaderNameAddition.empty() )
			{
				shaderName += " " + shaderSetupInfo.second.shaderNameAddition;
			}

			m_uiState.visualizationShaderComboBox.items.push_back( { shaderName, shaderSetupInfo } );
		}
		m_uiState.visualizationShaderComboBox.SetSelectedItemByValue( appState.modelState.activeShader.GetValue() );

		m_uiState.modelStates.lod.items.push_back( std::make_pair( "Auto", -1 ) );

		for( uint32_t lod = 0; lod < maxLod; ++lod )
		{
			m_uiState.modelStates.lod.items.push_back( std::make_pair( "Lod " + std::to_string( lod ), lod ) );
		}

		m_uiState.modelStates.lod.SetSelectedItemByValue( appState.modelState.selectedLod.GetValue() );
		m_uiState.modelStates.boundingBox = appState.modelState.modelBoundingBox.GetValue();

		if( m_playback.animationComboBox.items.empty() )
		{
			m_playback.animationComboBox.items.push_back( std::make_pair( "Rest Pose", "" ) );
			auto animationOwner = appState.modelState.activeAnimationOwner.GetValue();
			if( animationOwner )
			{
				cmf::Data* activeData = animationOwner->m_cmfData;

				for( const auto& animation : activeData->animations )
				{
					auto animationName = cmf::ToStdString( animation.name );
					m_playback.animationComboBox.items.push_back( std::make_pair( animationName, animationName ) );
					if( appState.modelState.currentAnimation.GetValue() == animationName )
					{
						m_playback.duration = animation.duration;
					}
				}
				m_playback.animationComboBox.SetSelectedItemByValue( appState.modelState.currentAnimation.GetValue() );
			}
		}
	}
	else
	{
		// default uninitialized values
		if( m_playback.animationComboBox.items.empty() )
		{
			m_playback.animationComboBox.items.push_back( std::make_pair( "Rest Pose", "" ) );
			m_playback.animationComboBox.SetSelectedItemByValue( appState.modelState.currentAnimation.GetValue() );
		}

		// polygon mode selection
		m_uiState.polygonModeComboBox.items = {
			{ "Fill", VK_POLYGON_MODE_FILL },
			{ "Line", VK_POLYGON_MODE_LINE },
			{ "Point", VK_POLYGON_MODE_POINT }
		};
		m_uiState.polygonModeComboBox.SetSelectedItemByValue( appState.modelState.polygonMode.GetValue() );
		m_uiState.filePath = "No file loaded";

		// visualization shader selection
		m_uiState.visualizationShaderComboBox.items.push_back( { "", {} } );
	}
}

void UIRenderer::OnChange( bool changed, std::function<void()> callback )
{
	if( changed )
	{
		callback();
	}
}

// NOLINTEND(cppcoreguidelines-pro-type-vararg)
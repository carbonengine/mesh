#include "uiRenderer.h"

#include <cmf/converters.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <tinyfiledialogs/tinyfiledialogs.h>

#include "appState.h"
#include "vulkan/shadercache.h"
#include "vulkan/vulkanerrors.h"

const float MENU_BAR_HEIGHT = 18.0f;
const float ANIMATION_PLAYER_HEIGHT = 36.0f;

enum class BoneColumn
{
	Name,
	Parent,
	Position,
	Rotation,
	Scale
};
enum class ChannelColumn
{
	Target,
	Type,
	Curve
};
enum class CurveColumn
{
	Interpolation,
	KnotType,
	ValueType,
	Dimensions,
	Knots
};
enum class AudioVertexColumn
{
	X,
	Y,
	Z
};

const ImVec4 LINK_COLOR( 0.4f, 0.6f, 1.0f, 1.0f );

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

		auto cmfContent = appState.modelState.animationOverride.GetValue();
		if( cmfContent == nullptr )
		{
			cmfContent = appState.cmfContent.GetValue();
		}

		if( !animationName.empty() && cmfContent != nullptr )
		{
			for( const auto& animation : cmfContent->m_cmfData->animations )
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

	appState.modelState.animationOverridePath.RegisterCallback( [this]( std::string animationPath, AppState& appState ) {
		appState.modelState.currentAnimation.SetValue( "" );
		appState.modelState.currentAnimationTime.SetValue( 0.0f );
		m_playback = Playback{};
	} );
}

const char* UIRenderer::FileOpenDialog( AppState& appState )
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

	CMFInfoWindow( appState );
	MeshDetailsWindow( appState );

	SetupPlaybackControls( appState );

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
	}
	ImGui::End();
}

void UIRenderer::MeshDetailsWindow( AppState& appState )
{
	bool open = true;

	float width = (float)appState.windowSize.GetValue().first;
	float height = (float)appState.windowSize.GetValue().second;

	float ySize = std::max( 1.0f, height - MENU_BAR_HEIGHT - ANIMATION_PLAYER_HEIGHT + 1 ); // +1 so we get an overlap of the borders

	// Pivot (1,0) anchors the top-right corner to the right edge of the viewport
	ImGui::SetNextWindowPos( ImVec2( width, MENU_BAR_HEIGHT ), ImGuiCond_Always, ImVec2( 1.0f, 0.0f ) );
	ImGui::SetNextWindowSizeConstraints( ImVec2( 0, ySize ), ImVec2( width, ySize ) );
	ImGui::SetNextWindowSize( ImVec2( width / 4.0f, ySize ), ImGuiCond_FirstUseEver );
	if( !ImGui::Begin( "Mesh Details", &open, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings ) )
	{
		ImGui::End();
		return;
	}

	auto* cmfContent = appState.cmfContent.GetValue().get();
	if( cmfContent == nullptr || cmfContent->m_cmfData == nullptr )
	{
		ImGui::Text( "No CMF loaded" );
		ImGui::End();
		return;
	}

	const auto& meshes = cmfContent->m_cmfData->meshes;
	if( meshes.empty() )
	{
		ImGui::Text( "No meshes" );
		ImGui::End();
		return;
	}

	m_meshDetailsState.selectedMeshIndex = std::min( m_meshDetailsState.selectedMeshIndex, (int)meshes.size() - 1 );
	const auto& mesh = meshes[m_meshDetailsState.selectedMeshIndex];

	ImGui::Text( "Mesh" );
	ImGui::SameLine();
	ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x );
	auto meshName = cmf::ToStdString( mesh.name );
	if( ImGui::BeginCombo( "##meshselect", meshName.c_str() ) )
	{
		for( int i = 0; i < (int)meshes.size(); ++i )
		{
			auto name = cmf::ToStdString( meshes[i].name );
			bool selected = ( i == m_meshDetailsState.selectedMeshIndex );
			if( ImGui::Selectable( name.c_str(), selected ) )
			{
				m_meshDetailsState.selectedMeshIndex = i;
				m_meshDetailsState.selectedLodIndex = 0;
				m_meshDetailsState.selectedMorphTargetIndex = 0;
				m_meshDetailsState.vertexAttributeFilter.clear();
				m_meshDetailsState.morphAttributeFilter.clear();
				m_meshDetailsState.boneColumnFilter.clear();
				m_meshDetailsState.audioVertexColumnFilter.clear();
			}
			if( selected )
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	if( mesh.lods.empty() )
	{
		ImGui::Text( "No LODs" );
		ImGui::End();
		return;
	}

	m_meshDetailsState.selectedLodIndex = std::min( m_meshDetailsState.selectedLodIndex, (int)mesh.lods.size() - 1 );

	ImGui::Text( "LOD" );
	ImGui::SameLine();
	ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x );
	std::string lodPreview = "LOD " + std::to_string( m_meshDetailsState.selectedLodIndex );
	if( ImGui::BeginCombo( "##lodselect", lodPreview.c_str() ) )
	{
		for( int i = 0; i < (int)mesh.lods.size(); ++i )
		{
			std::string label = "LOD " + std::to_string( i );
			bool selected = ( i == m_meshDetailsState.selectedLodIndex );
			if( ImGui::Selectable( label.c_str(), selected ) )
				m_meshDetailsState.selectedLodIndex = i;
			if( selected )
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	const auto& lod = mesh.lods[m_meshDetailsState.selectedLodIndex];

	if( ImGui::BeginTabBar( "##viewtabs" ) )
	{
		if( ImGui::BeginTabItem( "Hierarchy" ) )
		{
			RenderHierarchyTab( cmfContent );
			ImGui::EndTabItem();
		}
		ImGuiTabItemFlags vtdFlags = m_meshDetailsState.scrollToLinkedVertex ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
		if( ImGui::BeginTabItem( "Vertex Data", nullptr, vtdFlags ) )
		{
			RenderVertexDataTab( cmfContent, mesh, lod );
			ImGui::EndTabItem();
		}
		if( ImGui::BeginTabItem( "Index Data" ) )
		{
			RenderIndexDataTab( cmfContent, mesh, lod );
			ImGui::EndTabItem();
		}
		ImGuiTabItemFlags morphFlags = m_meshDetailsState.navigateToLinkedMorphTarget ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
		if( ImGui::BeginTabItem( "Morph Data", nullptr, morphFlags ) )
		{
			RenderMorphDataTab( cmfContent, mesh, lod );
			ImGui::EndTabItem();
		}
		ImGuiTabItemFlags bonesFlags = m_meshDetailsState.scrollToLinkedBone ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
		if( ImGui::BeginTabItem( "Bones", nullptr, bonesFlags ) )
		{
			RenderBonesTab( cmfContent, mesh );
			ImGui::EndTabItem();
		}
		if( ImGui::BeginTabItem( "Animations" ) )
		{
			RenderAnimationsTab( cmfContent );
			ImGui::EndTabItem();
		}
		if( ImGui::BeginTabItem( "Audio Occluder" ) )
		{
			RenderAudioOccluderTab( mesh );
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
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
		ImGui::Text( "Animation Path" );
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x );
		ImGui::InputText( "##animationPath", m_uiState.animationPath.data(), m_uiState.animationPath.size(), ImGuiInputTextFlags_ReadOnly );
		ImGui::SetItemTooltip( "%s", m_uiState.animationPath.data() );

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text( "Vertices" );
		ImGui::TableNextColumn();
		ImGui::Text( "%d", m_uiState.modelStates.totalVertexCount );
		ImGui::TableNextRow();


		ImGui::TableNextColumn();
		ImGui::Text( "Indices" );
		ImGui::TableNextColumn();
		ImGui::Text( "%d", m_uiState.modelStates.totalIndexCount );
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
		SetupCombo( "##visualiationMode", m_uiState.visualizationShaderComboBox, appState.modelState.visualizationShader );
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
		bool wireframeOverlay = std::all_of( appState.modelState.meshWireframeOverlay.begin(), appState.modelState.meshWireframeOverlay.end(), []( const State<bool>& state ) {
									return state.GetValue();
								} ) &&
			appState.modelState.meshWireframeOverlay.size() > 0;
		OnChange( ImGui::Checkbox( "##wireframecheckbox", &wireframeOverlay ), [&appState, &wireframeOverlay]() {
			std::for_each( appState.modelState.meshWireframeOverlay.begin(), appState.modelState.meshWireframeOverlay.end(), [wireframeOverlay]( State<bool>& state ) {
				state.SetValue( wireframeOverlay );
			} );
		} );
		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		ImGui::Text( "Audio Occlusion Mesh" );
		ImGui::TableNextColumn();
		bool audioOcclusion = std::all_of( appState.modelState.audioOcclusionMesh.begin(), appState.modelState.audioOcclusionMesh.end(), []( const State<bool>& state ) {
								  return state.GetValue();
							  } ) &&
			appState.modelState.audioOcclusionMesh.size() > 0;
		OnChange( ImGui::Checkbox( "##audioocclusioncheckbox", &audioOcclusion ), [&appState, &audioOcclusion]() {
			std::for_each( appState.modelState.audioOcclusionMesh.begin(), appState.modelState.audioOcclusionMesh.end(), [audioOcclusion]( State<bool>& state ) {
				state.SetValue( audioOcclusion );
			} );
		} );
		ImGui::TableNextRow();

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


void UIRenderer::SetupMeshView( const MeshUiState& mesh, AppState& appState )
{
	if( ImGui::TreeNode( mesh.name.c_str() ) )
	{
		if( ImGui::BeginTable( "##table", 2 ) )
		{
			ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_WidthFixed );
			ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_WidthStretch );
			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			ImGui::Text( "LOD" );
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x );
			SetupCombo( "##selectedlod", m_uiState.modelStates.lod, appState.modelState.selectedLod );

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text( "Vertex Count" );
			ImGui::TableNextColumn();
			ImGui::Text( "%d", mesh.vertexCount );

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text( "Index Count" );
			ImGui::TableNextColumn();
			ImGui::Text( "%d", mesh.indexCount );

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text( "Display" );
			ImGui::SetItemTooltip( "Toggles the \"%s\" mesh", mesh.name.c_str() );

			ImGui::TableNextColumn();
			bool display = mesh.display;
			OnChange( ImGui::Checkbox( "##displaycheckbox", &display ), [&appState, &mesh, &display]() {
				appState.modelState.meshVisibilityStates[mesh.meshIndex].SetValue( display );
			} );

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text( "Bounding Box" );
			ImGui::SetItemTooltip( "Toggles the bounding box for \"%s\" mesh", mesh.name.c_str() );

			ImGui::TableNextColumn();
			bool boundingBox = mesh.boundingBox;
			OnChange( ImGui::Checkbox( "##boundingboxcheckbox", &boundingBox ), [&appState, &mesh, &boundingBox]() {
				appState.modelState.meshBoundingBox[mesh.meshIndex].SetValue( boundingBox );
			} );
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text( "Wireframe Overlay" );
			ImGui::SetItemTooltip( "Toggles the wireframe overlay for the \"%s\" mesh", mesh.name.c_str() );

			ImGui::TableNextColumn();
			bool wireframeOverlay = mesh.wireframeOverlay;
			OnChange( ImGui::Checkbox( "##wireframeoverlaycheckbox", &wireframeOverlay ), [&appState, &mesh, &wireframeOverlay]() {
				appState.modelState.meshWireframeOverlay[mesh.meshIndex].SetValue( wireframeOverlay );
			} );
			ImGui::TableNextRow();

			ImGui::BeginDisabled( !mesh.hasAudioOcclusionMesh );
			ImGui::TableNextColumn();
			ImGui::Text( "Audio Occlusion Mesh" );
			ImGui::SetItemTooltip( "Toggles the audio occlusion mesh rendering for the \"%s\" mesh", mesh.name.c_str() );
			ImGui::TableNextColumn();
			bool audioOcclusion = mesh.audioOcclusionMesh;
			OnChange( ImGui::Checkbox( "##audioocclusionmeshcheckbox", &audioOcclusion ), [&appState, &mesh, &audioOcclusion]() {
				appState.modelState.audioOcclusionMesh[mesh.meshIndex].SetValue( audioOcclusion );
			} );
			ImGui::EndDisabled();

			ImGui::TableNextRow();

			ImGui::EndTable();
			if( !mesh.morphTargets.empty() )
			{
				if( ImGui::CollapsingHeader( "Morph Targets" ) )
				{
					uint32_t index = 0;
					for( const auto& morphTarget : mesh.morphTargets )
					{
						SetupMorphTarget( morphTarget, appState );
					}
				}
			}
		}
		ImGui::TreePop();
	}
}

void UIRenderer::SetupMorphTarget( const MorphTargetUiState& morphTarget, AppState& appState )
{
	ImGui::SeparatorText( morphTarget.name.c_str() );

	if( ImGui::BeginTable( "##table", 3 ) )
	{
		ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_WidthFixed, 50 );
		ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_WidthStretch );
		ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_WidthFixed, 30.0 );
		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		ImGui::Text( "Weight" );
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x );
		float weight = morphTarget.weight;
		OnChange( ImGui::SliderFloat( "##slider", &weight, 0.0f, 1.0f, "%.6f" ), [&appState, &morphTarget, &weight]() {
			appState.modelState.morphTargetWeight[morphTarget.index].SetValue( weight );
		} );
		ImGui::TableNextColumn();
		bool enabled = morphTarget.enabled;

		OnChange( ImGui::Checkbox( "##checkbox", &enabled ), [&appState, &morphTarget, &enabled]() {
			appState.modelState.morphTargetEnabled[morphTarget.index].SetValue( enabled );
		} );
		ImGui::SetItemTooltip( "Toggles the \"%s\" morph target", morphTarget.name.c_str() );
		ImGui::EndTable();
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
				auto filePath = FileOpenDialog( appState );
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
				auto filePath = FileOpenDialog( appState );
				if( filePath != nullptr )
				{
					appState.modelState.animationOverridePath.SetValue( std::string( filePath ) );
					appState.modelState.animationOverride.ForceSetValue( CmfContentLoader::LoadContentFromFile( filePath ) );
					appState.modelState.currentAnimation.SetValue( "" );
					appState.modelState.currentAnimationTime.SetValue( 0.0f );
				}
			}
			if( appState.cmfContent.GetValue() == nullptr )
			{
				ImGui::EndDisabled();
			}

			if( appState.modelState.animationOverride.GetValue() == nullptr )
			{
				ImGui::BeginDisabled();
			}

			if( ImGui::MenuItem( "Unload Animation Override" ) )
			{
				appState.modelState.animationOverridePath.SetValue( "" );
				appState.modelState.animationOverride.ForceSetValue( nullptr );
			}

			if( appState.modelState.animationOverride.GetValue() == nullptr )
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
			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}
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

	// update the current time
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
	if( ImGui::IsKeyDown( ImGuiMod_Ctrl ) && ImGui::IsKeyPressed( ImGuiKey_O ) )
	{
		auto filePath = FileOpenDialog( appState );
		if( filePath != nullptr )
		{
			appState.ResetModelState();
			appState.cmfContent.ForceSetValue( CmfContentLoader::LoadContentFromFile( filePath ) );
			appState.cmfPath.SetValue( std::string( filePath ) );
		}
	}
	if( ImGui::IsKeyDown( ImGuiMod_Ctrl ) && ImGui::IsKeyPressed( ImGuiKey_F ) )
	{
		appState.cameraTrigger.ForceSetValue( CameraTrigger::CAMERA_TRIGGER_FOCUS );
	}
}

void UIRenderer::UpdateUiState( AppState& appState )
{
	if( m_loadStatus == LoadStatus::SUCCESSFUL )
	{
		// reset all appStae items related to cmf
		appState.modelState.selectedLod.SetValue( 0 );
		appState.modelState.currentAnimation.SetValue( "" );
		appState.modelState.currentAnimationTime.SetValue( 0.0f );
		m_playback = Playback{};
	}

	m_uiState = UiState{};
	auto cmfContent = appState.cmfContent.GetValue();
	if( cmfContent != nullptr )
	{
		m_uiState.filePath = appState.cmfPath.GetValue();
		m_uiState.animationPath = appState.modelState.animationOverridePath.GetValue();
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
		for( const auto& mesh : cmfContent->m_cmfData->meshes )
		{
			if( meshIndex >= appState.modelState.meshVisibilityStates.size() )
			{
				break;
			}
			MeshUiState meshState{};
			meshState.meshIndex = meshIndex;
			meshState.name = cmf::ToStdString( mesh.name );
			meshState.lodCount = static_cast<uint32_t>( mesh.lods.size() );
			meshState.display = appState.modelState.meshVisibilityStates[meshIndex].GetValue();
			meshState.wireframeOverlay = appState.modelState.meshWireframeOverlay[meshIndex].GetValue();
			meshState.audioOcclusionMesh = appState.modelState.audioOcclusionMesh[meshIndex].GetValue();
			meshState.hasAudioOcclusionMesh = !mesh.audioOcclusionMesh.vertices.empty() && !mesh.audioOcclusionMesh.indices.empty();
			meshState.boundingBox = appState.modelState.meshBoundingBox[meshIndex].GetValue();

			maxLod = std::max( maxLod, mesh.lods.size() );

			for( const auto& lod : mesh.lods )
			{
				if( lod.vb.stride > 0 )
					meshState.vertexCount += lod.vb.size / lod.vb.stride;
				if( lod.ib.stride > 0 )
					meshState.indexCount += lod.ib.size / lod.ib.stride;
			}

			m_uiState.modelStates.totalVertexCount += meshState.vertexCount;
			m_uiState.modelStates.totalIndexCount += meshState.indexCount;

			for( const auto& morphTarget : mesh.morphTargets.targets )
			{
				if( morphIndex >= appState.modelState.morphTargetWeight.size() )
				{
					break;
				}
				MorphTargetUiState morphTargetState{};
				morphTargetState.name = cmf::ToStdString( morphTarget.name );
				morphTargetState.weight = appState.modelState.morphTargetWeight[morphIndex].GetValue();
				morphTargetState.enabled = appState.modelState.morphTargetEnabled[morphIndex].GetValue();
				morphTargetState.index = morphIndex++;
				meshState.morphTargets.push_back( morphTargetState );
			}

			m_uiState.modelStates.meshes.push_back( meshState );
			meshIndex++;
		}

		// visualization shader selection
		for( const auto& shaderName : appState.modelState.availableShaders.GetValue() )
		{
			m_uiState.visualizationShaderComboBox.items.push_back( { shaderName, shaderName } );
		}
		m_uiState.visualizationShaderComboBox.SetSelectedItemByValue( appState.modelState.visualizationShader.GetValue() );

		for( uint32_t lod = 0; lod < maxLod; ++lod )
		{
			m_uiState.modelStates.lod.items.push_back( std::make_pair( "Lod " + std::to_string( lod ), lod ) );
		}

		m_uiState.modelStates.lod.SetSelectedItemByValue( appState.modelState.selectedLod.GetValue() );
		m_uiState.modelStates.boundingBox = appState.modelState.modelBoundingBox.GetValue();

		if( m_playback.animationComboBox.items.empty() )
		{
			m_playback.animationComboBox.items.push_back( std::make_pair( "No Animation", "" ) );
			auto animationOverride = appState.modelState.animationOverride.GetValue();
			const auto& animations = animationOverride != nullptr ? animationOverride->m_cmfData->animations : cmfContent->m_cmfData->animations;

			for( const auto& animation : animations )
			{
				auto animationName = cmf::ToStdString( animation.name );
				m_playback.animationComboBox.items.push_back( std::make_pair( animationName, animationName ) );
			}
			m_playback.animationComboBox.SetSelectedItemByValue( appState.modelState.currentAnimation.GetValue() );
		}
	}
	else
	{
		// default uninitialized values
		if( m_playback.animationComboBox.items.empty() )
		{
			m_playback.animationComboBox.items.push_back( std::make_pair( "No Animation", "" ) );
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
		m_uiState.visualizationShaderComboBox.items.push_back( { "", "" } );
	}
}

void UIRenderer::OnChange( bool changed, std::function<void()> callback )
{
	if( changed )
	{
		callback();
	}
}

std::string UIRenderer::GetUsageFlagLabel( cmf::Usage usage, uint8_t usageIndex )
{
	std::string name;
	switch( usage )
	{
	case cmf::Usage::Position:
		name = "Position";
		break;
	case cmf::Usage::Normal:
		name = "Normal";
		break;
	case cmf::Usage::Tangent:
		name = "Tangent";
		break;
	case cmf::Usage::Binormal:
		name = "Binormal";
		break;
	case cmf::Usage::TexCoord:
		name = "TexCoord";
		break;
	case cmf::Usage::Color:
		name = "Color";
		break;
	case cmf::Usage::BoneIndices:
		name = "Bone Indices";
		break;
	case cmf::Usage::BoneWeights:
		name = "Bone Weights";
		break;
	case cmf::Usage::PackedTangent:
		name = "Packed Tangent";
		break;
	case cmf::Usage::PackedTangentLegacy:
		name = "Packed Tangent (Legacy)";
		break;
	default:
		name = "Unknown";
		break;
	}
	if( usageIndex > 0 )
		name += std::to_string( usageIndex );
	return name;
}

const char* UIRenderer::GetElementTypeName( cmf::ElementType type )
{
	static const char* names[] = {
		"Float32", "Float16", "UInt16Norm", "UInt16", "Int16Norm", "Int16", "UInt8Norm", "UInt8", "Int8Norm", "Int8"
	};
	int idx = (int)type;
	return idx >= 0 && idx < cmf::ElementTypeCount ? names[idx] : "Unknown";
}

void UIRenderer::RenderAttributeTable( const char* tableId, const uint8_t* vbData, uint32_t vertexCount, uint32_t stride, const std::vector<AttributeInfo>& attributes, int scrollToVertex )
{
	if( attributes.empty() )
	{
		ImGui::Text( "No readable vertex attributes" );
		return;
	}

	ImGuiTableFlags tableFlags =
		ImGuiTableFlags_Borders |
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_ScrollX |
		ImGuiTableFlags_ScrollY |
		ImGuiTableFlags_SizingFixedFit;

	int colCount = (int)attributes.size() + 1;
	ImVec2 outerSize( 0.0f, ImGui::GetContentRegionAvail().y );
	if( ImGui::BeginTable( tableId, colCount, tableFlags, outerSize ) )
	{
		if( scrollToVertex >= 0 )
		{
			float targetY = (float)scrollToVertex * ImGui::GetTextLineHeightWithSpacing();
			ImGui::SetScrollY( targetY );
		}

		ImGui::TableSetupScrollFreeze( 1, 1 );
		ImGui::TableSetupColumn( "Index", ImGuiTableColumnFlags_WidthFixed, 48.0f );
		for( const auto& attr : attributes )
			ImGui::TableSetupColumn( attr.name.c_str(), ImGuiTableColumnFlags_WidthStretch );
		ImGui::TableHeadersRow();

		ImGuiListClipper clipper;
		clipper.Begin( (int)vertexCount );
		while( clipper.Step() )
		{
			for( int vi = clipper.DisplayStart; vi < clipper.DisplayEnd; ++vi )
			{
				const uint8_t* vertexPtr = vbData + (size_t)vi * stride;

				ImGui::TableNextRow();
				if( vi == m_meshDetailsState.linkedVertexIndex )
					ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32( ImGuiCol_Header ) );

				ImGui::TableSetColumnIndex( 0 );
				ImGui::Text( "%d", vi );

				for( int ai = 0; ai < (int)attributes.size(); ++ai )
				{
					const auto& attr = attributes[ai];
					ImGui::TableSetColumnIndex( ai + 1 );

					if( attr.byteOffset + (uint32_t)attr.elementCount * (uint32_t)attr.conv.second > stride )
					{
						ImGui::TextUnformatted( "Stride Offset Missmatch" );
						continue;
					}

					if( attr.name.find( "Color" ) == 0 )
					{
						float c[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
						// Only read in RGB for the colors since we dont store alpha data
						for( uint8_t i = 0; i < attr.elementCount && i < 3; ++i )
						{
							const uint8_t* compPtr = vertexPtr + attr.byteOffset + i * attr.conv.second;
							float v = attr.conv.first.to( compPtr );
							c[i] = v < 0.0f ? 0.0f : ( v > 1.0f ? 1.0f : v );
						}
						float sz = ImGui::GetTextLineHeight();
						ImVec2 p = ImGui::GetCursorScreenPos();
						ImGui::GetWindowDrawList()->AddRectFilled(
							p, ImVec2( p.x + sz, p.y + sz ), IM_COL32( (int)( c[0] * 255 ), (int)( c[1] * 255 ), (int)( c[2] * 255 ), (int)( c[3] * 255 ) ) );
						ImGui::Dummy( ImVec2( sz + 4.0f, sz ) );
						ImGui::SameLine();
					}

					char valueBuf[128];
					char* ptr = valueBuf;
					char* end = valueBuf + sizeof( valueBuf );
					for( uint8_t c = 0; c < attr.elementCount && ptr < end - 1; ++c )
					{
						if( c > 0 && ptr < end - 3 )
						{
							*ptr++ = ' ';
							*ptr++ = ' ';
						}
						const uint8_t* compPtr = vertexPtr + attr.byteOffset + c * attr.conv.second;
						ptr += snprintf( ptr, end - ptr, "%.4f", attr.conv.first.to( compPtr ) );
					}
					ImGui::TextUnformatted( valueBuf );
				}
			}
		}
		clipper.End();
		ImGui::EndTable();
	}
}

void UIRenderer::RenderVertexDataTab( CmfContent* cmfContent, const cmf::Mesh& mesh, const cmf::MeshLod& lod )
{
	if( mesh.decl.empty() )
	{
		ImGui::Text( "No vertex declaration" );
		return;
	}
	if( lod.vb.stride == 0 || lod.vb.size == 0 )
	{
		ImGui::Text( "Empty vertex buffer" );
		return;
	}

	uint32_t vertexCount = lod.vb.size / lod.vb.stride;
	ImGui::Text( "Vertices: %u   Stride: %u bytes", vertexCount, lod.vb.stride );
	const uint8_t* vbData = cmfContent->Index( lod.vb.index, 0 ) + lod.vb.offset;

	// Get the labels for the vertex atributes
	auto allAttributes = BuildAttributes( mesh.decl );

	for( const auto& attr : allAttributes )
	{
		if( m_meshDetailsState.vertexAttributeFilter.find( attr.name ) == m_meshDetailsState.vertexAttributeFilter.end() )
			m_meshDetailsState.vertexAttributeFilter[attr.name] = true;
	}

	// Vertex attribute filter list
	if( ImGui::CollapsingHeader( "Filters" ) )
	{
		if( ImGui::Button( "Reset##vf" ) )
		{
			for( auto& [name, enabled] : m_meshDetailsState.vertexAttributeFilter )
				enabled = true;
		}
		for( const auto& attr : allAttributes )
		{
			bool enabled = m_meshDetailsState.vertexAttributeFilter[attr.name];
			if( ImGui::Checkbox( attr.name.c_str(), &enabled ) )
				m_meshDetailsState.vertexAttributeFilter[attr.name] = enabled;
		}
	}

	std::vector<AttributeInfo> filteredAttributes;
	for( const auto& attr : allAttributes )
	{
		if( m_meshDetailsState.vertexAttributeFilter[attr.name] )
			filteredAttributes.push_back( attr );
	}

	int scrollTarget = -1;
	if( m_meshDetailsState.scrollToLinkedVertex )
	{
		scrollTarget = m_meshDetailsState.linkedVertexIndex;
		m_meshDetailsState.scrollToLinkedVertex = false;
	}

	RenderAttributeTable( "##vertexdata", vbData, vertexCount, lod.vb.stride, filteredAttributes, scrollTarget );
}

void UIRenderer::RenderIndexDataTab( CmfContent* cmfContent, const cmf::Mesh& mesh, const cmf::MeshLod& lod )
{
	if( lod.ib.stride == 0 || lod.ib.size == 0 )
	{
		ImGui::Text( "Empty index buffer" );
		return;
	}

	uint32_t indexCount = lod.ib.size / lod.ib.stride;
	const uint8_t* ibData = cmfContent->Index( lod.ib.index, 0 ) + lod.ib.offset;
	cmf::IndexConverter indexConv( lod.ib.stride );
	bool isTriangleList = mesh.topology == cmf::MeshTopology::TriangleList;

	if( isTriangleList )
	{
		ImGui::RadioButton( "Raw", &m_meshDetailsState.indexViewMode, 1 );
		ImGui::SameLine();
		ImGui::RadioButton( "Triangles", &m_meshDetailsState.indexViewMode, 0 );
	}

	auto indexSelectable = [&]( uint32_t vertexIdx, int uniqueId ) {
		char buf[16];
		snprintf( buf, sizeof( buf ), "%u", vertexIdx );
		ImGui::PushID( uniqueId );
		bool isSelected = ( (int)vertexIdx == m_meshDetailsState.selectedIndexValue );
		ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( 0.4f, 0.6f, 1.0f, 1.0f ) );
		ImGui::Selectable( buf, isSelected, ImGuiSelectableFlags_None );
		ImGui::PopStyleColor();
		if( ImGui::IsItemClicked( ImGuiMouseButton_Left ) )
			m_meshDetailsState.selectedIndexValue = (int)vertexIdx;
		if( ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) )
		{
			m_meshDetailsState.linkedVertexIndex = (int)vertexIdx;
			m_meshDetailsState.scrollToLinkedVertex = true;
		}
		ImGui::PopID();
	};

	if( isTriangleList && m_meshDetailsState.indexViewMode == 0 )
	{
		uint32_t triangleCount = indexCount / 3;
		ImGui::Text( "Triangles: %u   Indices: %u", triangleCount, indexCount );

		ImGuiTableFlags tableFlags =
			ImGuiTableFlags_Borders |
			ImGuiTableFlags_RowBg |
			ImGuiTableFlags_ScrollY |
			ImGuiTableFlags_SizingFixedFit;

		ImVec2 outerSize( 0.0f, ImGui::GetContentRegionAvail().y );
		if( ImGui::BeginTable( "##indexdata", 4, tableFlags, outerSize ) )
		{
			ImGui::TableSetupScrollFreeze( 0, 1 );
			ImGui::TableSetupColumn( "Triangle", ImGuiTableColumnFlags_WidthFixed, 72.0f );
			ImGui::TableSetupColumn( "V0", ImGuiTableColumnFlags_WidthStretch );
			ImGui::TableSetupColumn( "V1", ImGuiTableColumnFlags_WidthStretch );
			ImGui::TableSetupColumn( "V2", ImGuiTableColumnFlags_WidthStretch );
			ImGui::TableHeadersRow();

			ImGuiListClipper clipper;
			clipper.Begin( (int)triangleCount );
			while( clipper.Step() )
			{
				for( int ti = clipper.DisplayStart; ti < clipper.DisplayEnd; ++ti )
				{
					const uint8_t* base = ibData + (size_t)ti * 3 * lod.ib.stride;
					uint32_t v0 = indexConv( base );
					uint32_t v1 = indexConv( base + lod.ib.stride );
					uint32_t v2 = indexConv( base + 2 * lod.ib.stride );

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex( 0 );
					ImGui::Text( "%d", ti );
					ImGui::TableSetColumnIndex( 1 );
					indexSelectable( v0, ti * 3 );
					ImGui::TableSetColumnIndex( 2 );
					indexSelectable( v1, ti * 3 + 1 );
					ImGui::TableSetColumnIndex( 3 );
					indexSelectable( v2, ti * 3 + 2 );
				}
			}
			clipper.End();
			ImGui::EndTable();
		}
	}
	else
	{
		ImGui::Text( "Indices: %u", indexCount );

		ImGuiTableFlags tableFlags =
			ImGuiTableFlags_Borders |
			ImGuiTableFlags_RowBg |
			ImGuiTableFlags_ScrollY |
			ImGuiTableFlags_SizingFixedFit;

		ImVec2 outerSize( 0.0f, ImGui::GetContentRegionAvail().y );
		if( ImGui::BeginTable( "##indexdataraw", 2, tableFlags, outerSize ) )
		{
			ImGui::TableSetupScrollFreeze( 0, 1 );
			ImGui::TableSetupColumn( "Index", ImGuiTableColumnFlags_WidthFixed, 72.0f );
			ImGui::TableSetupColumn( "Value", ImGuiTableColumnFlags_WidthStretch );
			ImGui::TableHeadersRow();

			ImGuiListClipper clipper;
			clipper.Begin( (int)indexCount );
			while( clipper.Step() )
			{
				for( int ii = clipper.DisplayStart; ii < clipper.DisplayEnd; ++ii )
				{
					uint32_t v = indexConv( ibData + (size_t)ii * lod.ib.stride );

					ImGui::TableNextRow();
					if( (int)v == m_meshDetailsState.selectedIndexValue )
						ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg1, ImGui::GetColorU32( ImGuiCol_Header ) );

					ImGui::TableSetColumnIndex( 0 );
					ImGui::Text( "%d", ii );
					ImGui::TableSetColumnIndex( 1 );
					indexSelectable( v, ii );
				}
			}
			clipper.End();
			ImGui::EndTable();
		}
	}
}

void UIRenderer::RenderMorphDataTab( CmfContent* cmfContent, const cmf::Mesh& mesh, const cmf::MeshLod& lod )
{
	const auto& morphTargets = mesh.morphTargets;
	if( morphTargets.targets.empty() )
	{
		ImGui::Text( "No morph targets" );
		return;
	}
	if( morphTargets.decl.empty() )
	{
		ImGui::Text( "No morph target vertex declaration" );
		return;
	}

	if( m_meshDetailsState.navigateToLinkedMorphTarget )
	{
		m_meshDetailsState.selectedMorphTargetIndex = m_meshDetailsState.linkedMorphTargetIndex;
		m_meshDetailsState.navigateToLinkedMorphTarget = false;
	}

	m_meshDetailsState.selectedMorphTargetIndex = std::min(
		m_meshDetailsState.selectedMorphTargetIndex,
		(int)morphTargets.targets.size() - 1 );

	auto morphName = cmf::ToStdString( morphTargets.targets[m_meshDetailsState.selectedMorphTargetIndex].name );
	ImGui::Text( "Morph Target" );
	ImGui::SameLine();
	ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x );
	if( ImGui::BeginCombo( "##morphselect", morphName.c_str() ) )
	{
		for( int i = 0; i < (int)morphTargets.targets.size(); ++i )
		{
			auto name = cmf::ToStdString( morphTargets.targets[i].name );
			bool selected = ( i == m_meshDetailsState.selectedMorphTargetIndex );
			if( ImGui::Selectable( name.c_str(), selected ) )
				m_meshDetailsState.selectedMorphTargetIndex = i;
			if( selected )
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	if( m_meshDetailsState.selectedMorphTargetIndex >= (int)lod.morphTargets.size() )
	{
		ImGui::Text( "No LOD morph data for this morph target" );
		return;
	}

	const auto& morphLod = lod.morphTargets[m_meshDetailsState.selectedMorphTargetIndex];
	if( morphLod.vb.stride == 0 || morphLod.vb.size == 0 )
	{
		ImGui::Text( "Empty morph target buffer" );
		return;
	}

	uint32_t vertexCount = morphLod.vb.size / morphLod.vb.stride;
	ImGui::Text( "Vertices: %u   Stride: %u bytes", vertexCount, morphLod.vb.stride );
	const uint8_t* vbData = cmfContent->Index( morphLod.vb.index, 0 ) + morphLod.vb.offset;

	auto allAttributes = BuildAttributes( morphTargets.decl );

	for( const auto& attr : allAttributes )
	{
		if( m_meshDetailsState.morphAttributeFilter.find( attr.name ) == m_meshDetailsState.morphAttributeFilter.end() )
			m_meshDetailsState.morphAttributeFilter[attr.name] = true;
	}

	if( ImGui::CollapsingHeader( "Filters" ) )
	{
		if( ImGui::Button( "Reset##mf" ) )
		{
			for( auto& [name, enabled] : m_meshDetailsState.morphAttributeFilter )
				enabled = true;
		}
		for( const auto& attr : allAttributes )
		{
			bool enabled = m_meshDetailsState.morphAttributeFilter[attr.name];
			if( ImGui::Checkbox( attr.name.c_str(), &enabled ) )
				m_meshDetailsState.morphAttributeFilter[attr.name] = enabled;
		}
	}

	std::vector<AttributeInfo> filteredAttributes;
	for( const auto& attr : allAttributes )
	{
		if( m_meshDetailsState.morphAttributeFilter[attr.name] )
			filteredAttributes.push_back( attr );
	}

	RenderAttributeTable( "##morphdata", vbData, vertexCount, morphLod.vb.stride, filteredAttributes, -1 );
}

void UIRenderer::RenderBonesTab( CmfContent* cmfContent, const cmf::Mesh& mesh )
{
	const auto& skeletons = cmfContent->m_cmfData->skeletons;
	bool hasSkeleton = mesh.skeleton != 0xff && (size_t)mesh.skeleton < skeletons.size();
	bool hasBoneBindings = !mesh.boneBindings.empty();

	if( !hasSkeleton && !hasBoneBindings )
	{
		ImGui::Text( "No bone data" );
		return;
	}

	if( hasBoneBindings )
	{
		if( hasSkeleton )
			ImGui::SeparatorText( "Bone Bindings" );
		ImGui::Text( "Bindings: %u", (uint32_t)mesh.boneBindings.size() );

		float lineHeight = ImGui::GetTextLineHeightWithSpacing();
		float contentHeight = (float)( mesh.boneBindings.size() + 1 ) * lineHeight;
		float halfWindowHeight = ImGui::GetContentRegionAvail().y * 0.5f;
		float bindingsHeight = hasSkeleton ? std::min( contentHeight, halfWindowHeight ) : ImGui::GetContentRegionAvail().y;

		ImGuiTableFlags tableFlags =
			ImGuiTableFlags_Borders |
			ImGuiTableFlags_RowBg |
			ImGuiTableFlags_ScrollY |
			ImGuiTableFlags_SizingFixedFit;

		if( ImGui::BeginTable( "##bonebindingstable", 2, tableFlags, ImVec2( 0.0f, bindingsHeight ) ) )
		{
			ImGui::TableSetupScrollFreeze( 0, 1 );
			ImGui::TableSetupColumn( "Index", ImGuiTableColumnFlags_WidthFixed, 48.0f );
			ImGui::TableSetupColumn( "Name", ImGuiTableColumnFlags_WidthStretch );
			ImGui::TableHeadersRow();

			ImGuiListClipper clipper;
			clipper.Begin( (int)mesh.boneBindings.size() );
			while( clipper.Step() )
			{
				for( int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i )
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex( 0 );
					ImGui::Text( "%d", i );
					ImGui::TableSetColumnIndex( 1 );
					ImGui::TextUnformatted( cmf::ToStdString( mesh.boneBindings[i].name ).c_str() );
				}
			}
			clipper.End();
			ImGui::EndTable();
		}
	}

	if( hasSkeleton )
	{
		const auto& skeleton = skeletons[mesh.skeleton];
		std::string skelName = cmf::ToStdString( skeleton.name );
		ImGui::Text( "Skeleton: %s  Index: %u  Bones: %u", skelName.c_str(), (uint32_t)mesh.skeleton, (uint32_t)skeleton.bones.size() );

		static const char* allBoneCols[] = { "Name", "Parent", "Position", "Rotation", "Scale" };
		for( const auto& col : allBoneCols )
		{
			if( m_meshDetailsState.boneColumnFilter.find( col ) == m_meshDetailsState.boneColumnFilter.end() )
				m_meshDetailsState.boneColumnFilter[col] = true;
		}

		if( ImGui::CollapsingHeader( "Filters" ) )
		{
			if( ImGui::Button( "Reset##bf" ) )
			{
				for( auto& [name, enabled] : m_meshDetailsState.boneColumnFilter )
					enabled = true;
			}
			for( const auto& col : allBoneCols )
			{
				bool enabled = m_meshDetailsState.boneColumnFilter[col];
				if( ImGui::Checkbox( col, &enabled ) )
					m_meshDetailsState.boneColumnFilter[col] = enabled;
			}
		}

		std::vector<int> activeColIndices;
		for( int i = 0; i < 5; ++i )
		{
			if( m_meshDetailsState.boneColumnFilter[allBoneCols[i]] )
				activeColIndices.push_back( i );
		}

		ImGuiTableFlags tableFlags =
			ImGuiTableFlags_Borders |
			ImGuiTableFlags_RowBg |
			ImGuiTableFlags_ScrollX |
			ImGuiTableFlags_ScrollY |
			ImGuiTableFlags_SizingFixedFit;

		int scrollToBone = -1;
		if( m_meshDetailsState.scrollToLinkedBone )
		{
			scrollToBone = m_meshDetailsState.linkedBoneIndex;
			m_meshDetailsState.scrollToLinkedBone = false;
		}

		int colCount = (int)activeColIndices.size() + 1;
		if( ImGui::BeginTable( "##boneslist", colCount, tableFlags, ImVec2( 0.0f, ImGui::GetContentRegionAvail().y ) ) )
		{
			if( scrollToBone >= 0 )
				ImGui::SetScrollY( (float)scrollToBone * ImGui::GetTextLineHeightWithSpacing() );

			ImGui::TableSetupScrollFreeze( 1, 1 );
			ImGui::TableSetupColumn( "Index", ImGuiTableColumnFlags_WidthFixed, 48.0f );
			for( int ci : activeColIndices )
				ImGui::TableSetupColumn( allBoneCols[ci], ImGuiTableColumnFlags_WidthStretch );
			ImGui::TableHeadersRow();

			ImGuiListClipper clipper;
			clipper.Begin( (int)skeleton.bones.size() );
			while( clipper.Step() )
			{
				for( int bi = clipper.DisplayStart; bi < clipper.DisplayEnd; ++bi )
				{
					uint32_t parentIdx = ( (size_t)bi < skeleton.parents.size() ) ? skeleton.parents[bi] : 0xffffffff;
					bool hasTransform = (size_t)bi < skeleton.restTransforms.size();

					ImGui::TableNextRow();
					if( bi == m_meshDetailsState.linkedBoneIndex )
						ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32( ImGuiCol_Header ) );

					ImGui::TableSetColumnIndex( 0 );
					ImGui::Text( "%d", bi );

					char buf[128];
					for( int col = 0; col < (int)activeColIndices.size(); ++col )
					{
						ImGui::TableSetColumnIndex( col + 1 );
						switch( activeColIndices[col] )
						{
						case 0: // Name
							ImGui::TextUnformatted( cmf::ToStdString( skeleton.bones[bi] ).c_str() );
							break;
						case 1: // Parent
							if( parentIdx == (uint32_t)bi || parentIdx >= (uint32_t)skeleton.bones.size() )
							{
								ImGui::TextUnformatted( "-" );
							}
							else
							{
								std::string parentName = cmf::ToStdString( skeleton.bones[parentIdx] );
								ImGui::PushID( bi );
								ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( 0.4f, 0.6f, 1.0f, 1.0f ) );
								ImGui::Selectable( parentName.c_str(), false );
								ImGui::PopStyleColor();
								if( ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) )
								{
									m_meshDetailsState.linkedBoneIndex = (int)parentIdx;
									m_meshDetailsState.scrollToLinkedBone = true;
								}
								ImGui::PopID();
							}
							break;
						case 2: // Position
							if( hasTransform )
							{
								const auto& p = skeleton.restTransforms[bi].position;
								snprintf( buf, sizeof( buf ), "%.4f  %.4f  %.4f", p.x, p.y, p.z );
								ImGui::TextUnformatted( buf );
							}
							break;
						case 3: // Rotation
							if( hasTransform )
							{
								const auto& r = skeleton.restTransforms[bi].rotation;
								snprintf( buf, sizeof( buf ), "%.4f  %.4f  %.4f  %.4f", r.x, r.y, r.z, r.w );
								ImGui::TextUnformatted( buf );
							}
							break;
						case 4: // Scale
							if( hasTransform )
							{
								const auto& s = skeleton.restTransforms[bi].scale;
								snprintf( buf, sizeof( buf ), "%.4f  %.4f  %.4f", s.x, s.y, s.z );
								ImGui::TextUnformatted( buf );
							}
							break;
						}
					}
				}
			}
			clipper.End();
			ImGui::EndTable();
		}
	}
}

void UIRenderer::RenderHierarchyTab( CmfContent* cmfContent )
{
	if( !ImGui::BeginChild( "##hierarchyscroll", ImVec2( 0.0f, 0.0f ) ) )
	{
		ImGui::EndChild();
		return;
	}

	const auto& meshes = cmfContent->m_cmfData->meshes;
	const auto& skeletons = cmfContent->m_cmfData->skeletons;

	std::function<void( int, const cmf::Skeleton&, const std::vector<std::vector<int>>& )> renderBone = [&]( int bi, const cmf::Skeleton& skeleton, const std::vector<std::vector<int>>& children ) {
		std::string boneName = cmf::ToStdString( skeleton.bones[bi] );
		bool isLeaf = children[bi].empty();
		ImGuiTreeNodeFlags flags = isLeaf ? ( ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen ) : ImGuiTreeNodeFlags_None;

		ImGui::PushStyleColor( ImGuiCol_Text, LINK_COLOR );
		bool open = ImGui::TreeNodeEx( (void*)(intptr_t)bi, flags, "%s", boneName.c_str() );
		ImGui::PopStyleColor();
		if( ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) )
		{
			m_meshDetailsState.linkedBoneIndex = bi;
			m_meshDetailsState.scrollToLinkedBone = true;
		}
		if( open && !isLeaf )
		{
			for( int child : children[bi] )
				renderBone( child, skeleton, children );
			ImGui::TreePop();
		}
	};

	std::function<void( const cmf::Skeleton& )> renderSkeleton = [&]( const cmf::Skeleton& skeleton ) {
		int boneCount = (int)skeleton.bones.size();
		std::vector<std::vector<int>> children( boneCount );
		std::vector<int> roots;

		for( int i = 0; i < boneCount; ++i )
		{
			uint32_t parent = ( (size_t)i < skeleton.parents.size() ) ? skeleton.parents[i] : 0xffffffff;
			if( parent == (uint32_t)i || parent >= (uint32_t)boneCount )
				roots.push_back( i );
			else
				children[parent].push_back( i );
		}

		for( int root : roots )
		{
			renderBone( root, skeleton, children );
		}
	};

	if( !skeletons.empty() )
	{
		std::string skelSectionHeader = "Skeletons (" + std::to_string( skeletons.size() ) + ")";
		if( ImGui::TreeNodeEx( skelSectionHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen ) )
		{
			for( int si = 0; si < (int)skeletons.size(); ++si )
			{
				ImGui::PushID( si );
				const auto& skeleton = skeletons[si];
				std::string skelHeader = "[" + std::to_string( si ) + "] " + cmf::ToStdString( skeleton.name ) +
					" (" + std::to_string( skeleton.bones.size() ) + " bones)";

				if( ImGui::TreeNodeEx( skelHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen ) )
				{
					renderSkeleton( skeleton );
					ImGui::TreePop();
				}
				ImGui::PopID();
			}
			ImGui::TreePop();
		}
	}

	std::string meshesSectionHeader = "Meshes (" + std::to_string( meshes.size() ) + ")";
	if( ImGui::TreeNodeEx( meshesSectionHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen ) )
	{
		for( int mi = 0; mi < (int)meshes.size(); ++mi )
		{
			ImGui::PushID( mi );
			const auto& mesh = meshes[mi];
			bool isCurrentMesh = ( mi == m_meshDetailsState.selectedMeshIndex );
			ImGuiTreeNodeFlags meshFlags = isCurrentMesh ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None;
			std::string meshHeader = "Mesh: " + cmf::ToStdString( mesh.name );
			if( ImGui::TreeNodeEx( "##meshnode", meshFlags, "%s", meshHeader.c_str() ) )
			{
				{
					const auto& b = mesh.bounds;
					auto sz = b.Size();
					ImGui::Text( "Bounds Min:  %.4f  %.4f  %.4f", b.m_min.x, b.m_min.y, b.m_min.z );
					ImGui::Text( "Bounds Max:  %.4f  %.4f  %.4f", b.m_max.x, b.m_max.y, b.m_max.z );
					ImGui::Text( "Bounds Size: %.4f  %.4f  %.4f", sz.x, sz.y, sz.z );
				}
				bool hasSkeleton = mesh.skeleton != 0xff && (size_t)mesh.skeleton < skeletons.size();
				if( hasSkeleton )
				{
					const auto& skeleton = skeletons[mesh.skeleton];
					std::string skelHeader = "Skeleton[" + std::to_string( mesh.skeleton ) + "]: " + cmf::ToStdString( skeleton.name ) +
						" (" + std::to_string( skeleton.bones.size() ) + " bones)";
					if( ImGui::TreeNodeEx( skelHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen ) )
					{
						renderSkeleton( skeleton );
						ImGui::TreePop();
					}
				}
				if( !mesh.morphTargets.targets.empty() )
				{
					std::string morphHeader = "Morph Targets (" + std::to_string( mesh.morphTargets.targets.size() ) + ")";
					if( ImGui::TreeNode( morphHeader.c_str() ) )
					{
						if( !mesh.morphTargets.decl.empty() && ImGui::TreeNode( "Declaration" ) )
						{
							for( const auto& elem : mesh.morphTargets.decl )
							{
								std::string attrName = GetUsageFlagLabel( elem.usage, elem.usageIndex );
								const char* typeName = GetElementTypeName( elem.type );
								ImGui::TreeNodeEx( attrName.c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen );
								ImGui::SameLine();
								ImGui::TextDisabled( "(%s x%u)", typeName, (uint32_t)elem.elementCount );
							}
							ImGui::TreePop();
						}
						for( int ti = 0; ti < (int)mesh.morphTargets.targets.size(); ++ti )
						{
							const auto& target = mesh.morphTargets.targets[ti];
							std::string name = cmf::ToStdString( target.name );
							ImGui::PushStyleColor( ImGuiCol_Text, LINK_COLOR );
							bool morphOpen = ImGui::TreeNode( name.c_str() );
							ImGui::PopStyleColor();
							if( ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) )
							{
								m_meshDetailsState.linkedMorphTargetIndex = ti;
								m_meshDetailsState.navigateToLinkedMorphTarget = true;
							}
							if( morphOpen )
							{
								ImGui::Text( "Max Displacement: %.4f", target.maxDisplacement );
								for( int li = 0; li < (int)mesh.lods.size(); ++li )
								{
									const auto& lod = mesh.lods[li];
									if( ti < (int)lod.morphTargets.size() )
									{
										const auto& morphLod = lod.morphTargets[ti];
										uint32_t vtxCount = morphLod.vb.stride > 0 ? morphLod.vb.size / morphLod.vb.stride : 0;
										ImGui::Text( "LOD %d: %u vertices", li, vtxCount );
									}
								}
								ImGui::TreePop();
							}
						}
						ImGui::TreePop();
					}
				}
				if( !mesh.areas.empty() )
				{
					std::string areasHeader = "Mesh Areas (" + std::to_string( mesh.areas.size() ) + ")";
					if( ImGui::TreeNode( areasHeader.c_str() ) )
					{
						for( const auto& area : mesh.areas )
						{
							ImGui::PushID( &area );
							std::string areaName = cmf::ToStdString( area.name );
							if(areaName.empty())
							{
								areaName = "Unnamed Area";
							}
							if( ImGui::TreeNode( areaName.c_str() ) )
							{
								ImGui::Text( "Affected by Bones: %s", area.affectedByBones ? "Yes" : "No" );
								ImGui::Text( "Affected by Morphs: %s", area.affectedByMorphTargets ? "Yes" : "No" );
								if( !area.bones.empty() )
								{
									std::string boneList = "Bones:";
									for( uint16_t bi : area.bones )
										boneList += " " + std::to_string( bi );
									ImGui::TextUnformatted( boneList.c_str() );
								}
								ImGui::TreePop();
							}
							ImGui::PopID();
						}
						ImGui::TreePop();
					}
				}
				if( !mesh.lods.empty() )
				{
					std::string lodsHeader = "LODs (" + std::to_string( mesh.lods.size() ) + ")";
					if( ImGui::TreeNode( lodsHeader.c_str() ) )
					{
						for( int i = 0; i < (int)mesh.lods.size(); ++i )
						{
							const auto& lod = mesh.lods[i];
							std::string lodLabel;
							if( lod.threshold == cmf::MeshLod::MAX_THRESHOLD )
								lodLabel = "LOD " + std::to_string( i ) + " (base)";
							else
								lodLabel = "LOD " + std::to_string( i ) + " (threshold: " + std::to_string( lod.threshold ) + "px)";

							ImGui::PushStyleColor( ImGuiCol_Text, LINK_COLOR );
							bool lodOpen = ImGui::TreeNode( lodLabel.c_str() );
							ImGui::PopStyleColor();
							if( ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) )
							{
								m_meshDetailsState.selectedLodIndex = i;
								m_meshDetailsState.scrollToLinkedVertex = true;
							}
							if( lodOpen )
							{
								uint32_t vertexCount = lod.vb.stride > 0 ? lod.vb.size / lod.vb.stride : 0;
								uint32_t indexCount = lod.ib.stride > 0 ? lod.ib.size / lod.ib.stride : 0;
								ImGui::Text( "Vertices: %u", vertexCount );
								ImGui::Text( "Indices: %u", indexCount );
								if( !lod.areas.empty() )
									ImGui::Text( "Areas: %u", (uint32_t)lod.areas.size() );
								if( !lod.morphTargets.empty() )
									ImGui::Text( "Morph Targets: %u", (uint32_t)lod.morphTargets.size() );
								ImGui::TreePop();
							}
						}
						ImGui::TreePop();
					}
				}
				if( !mesh.audioOcclusionMesh.vertices.empty() )
				{
					if( ImGui::TreeNode( "Audio Occlusion Mesh" ) )
					{
						ImGui::Text( "Vertices: %u", (uint32_t)mesh.audioOcclusionMesh.vertices.size() );
						ImGui::Text( "Indices: %u", (uint32_t)mesh.audioOcclusionMesh.indices.size() );
						ImGui::TreePop();
					}
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		ImGui::TreePop(); // Meshes
	}
	ImGui::EndChild();
}

void UIRenderer::RenderAnimationsTab( CmfContent* cmfContent )
{
	const auto& animations = cmfContent->m_cmfData->animations;
	if( animations.empty() )
	{
		ImGui::Text( "No animations" );
		return;
	}

	m_meshDetailsState.selectedAnimationIndex = std::min(
		m_meshDetailsState.selectedAnimationIndex,
		(int)animations.size() - 1 );

	auto animName = cmf::ToStdString( animations[m_meshDetailsState.selectedAnimationIndex].name );
	ImGui::Text( "Animation" );
	ImGui::SameLine();
	ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x );
	if( ImGui::BeginCombo( "##animselect", animName.c_str() ) )
	{
		for( int i = 0; i < (int)animations.size(); ++i )
		{
			auto name = cmf::ToStdString( animations[i].name );
			bool selected = ( i == m_meshDetailsState.selectedAnimationIndex );
			if( ImGui::Selectable( name.c_str(), selected ) )
				m_meshDetailsState.selectedAnimationIndex = i;
			if( selected )
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	const auto& anim = animations[m_meshDetailsState.selectedAnimationIndex];
	ImGui::Text( "Duration: %.4f s", anim.duration );
	ImGui::Text( "Channels: %u   Curves: %u", (uint32_t)anim.channels.size(), (uint32_t)anim.curves.size() );

	ImGui::Separator();

	if( ImGui::BeginTabBar( "##animsubtabs" ) )
	{
		if( ImGui::BeginTabItem( "Channels" ) )
		{
			RenderAnimationChannelsSubTab( anim );
			ImGui::EndTabItem();
		}

		ImGuiTabItemFlags curvesTabFlags = m_meshDetailsState.navigateToLinkedCurve ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
		if( ImGui::BeginTabItem( "Curves", nullptr, curvesTabFlags ) )
		{
			RenderAnimationCurvesSubTab( anim );
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}
}

void UIRenderer::RenderAnimationChannelsSubTab( const cmf::Animation& anim )
{
	if( anim.channels.empty() )
	{
		ImGui::Text( "No channels" );
		return;
	}

	static const char* allChannelCols[] = { "Target", "Type", "Curve" };
	for( const auto& col : allChannelCols )
		if( m_meshDetailsState.channelColumnFilter.find( col ) == m_meshDetailsState.channelColumnFilter.end() )
			m_meshDetailsState.channelColumnFilter[col] = true;

	if( ImGui::CollapsingHeader( "Filters" ) )
	{
		if( ImGui::Button( "Reset##chf" ) )
			for( auto& [name, enabled] : m_meshDetailsState.channelColumnFilter )
				enabled = true;
		for( const auto& col : allChannelCols )
		{
			bool enabled = m_meshDetailsState.channelColumnFilter[col];
			if( ImGui::Checkbox( col, &enabled ) )
				m_meshDetailsState.channelColumnFilter[col] = enabled;
		}
	}

	std::vector<ChannelColumn> activeChannelCols;
	for( int i = 0; i < 3; ++i )
		if( m_meshDetailsState.channelColumnFilter[allChannelCols[i]] )
			activeChannelCols.push_back( static_cast<ChannelColumn>( i ) );

	if( activeChannelCols.empty() )
		return;

	ImGuiTableFlags tableFlags =
		ImGuiTableFlags_Borders |
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_ScrollX |
		ImGuiTableFlags_ScrollY |
		ImGuiTableFlags_SizingFixedFit;

	if( ImGui::BeginTable( "##channelstable", (int)activeChannelCols.size(), tableFlags, ImVec2( 0.0f, ImGui::GetContentRegionAvail().y ) ) )
	{
		ImGui::TableSetupScrollFreeze( 0, 1 );
		for( ChannelColumn ci : activeChannelCols )
		{
			if( ci == ChannelColumn::Target )
				ImGui::TableSetupColumn( allChannelCols[(int)ci], ImGuiTableColumnFlags_WidthStretch );
			else
				ImGui::TableSetupColumn( allChannelCols[(int)ci], ImGuiTableColumnFlags_WidthFixed, ci == ChannelColumn::Curve ? 48.0f : 120.0f );
		}
		ImGui::TableHeadersRow();

		ImGuiListClipper clipper;
		clipper.Begin( (int)anim.channels.size() );
		while( clipper.Step() )
		{
			for( int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i )
			{
				const auto& channel = anim.channels[i];

				const char* typeName = "Unknown";
				switch( channel.targetType )
				{
				case cmf::AnimationChannelTargetType::BonePosition:
					typeName = "Bone Position";
					break;
				case cmf::AnimationChannelTargetType::BoneRotation:
					typeName = "Bone Rotation";
					break;
				case cmf::AnimationChannelTargetType::BoneScale:
					typeName = "Bone Scale";
					break;
				case cmf::AnimationChannelTargetType::MorphTarget:
					typeName = "Morph Target";
					break;
				case cmf::AnimationChannelTargetType::Other:
					typeName = "Other";
					break;
				}

				ImGui::TableNextRow();
				for( int col = 0; col < (int)activeChannelCols.size(); ++col )
				{
					ImGui::TableSetColumnIndex( col );
					switch( activeChannelCols[col] )
					{
					case ChannelColumn::Target:
						ImGui::TextUnformatted( cmf::ToStdString( channel.target ).c_str() );
						break;
					case ChannelColumn::Type:
						ImGui::TextUnformatted( typeName );
						break;
					case ChannelColumn::Curve: {
						char curveBuf[16];
						snprintf( curveBuf, sizeof( curveBuf ), "%u", channel.curveIndex );
						ImGui::PushID( i );
						ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( 0.4f, 0.6f, 1.0f, 1.0f ) );
						ImGui::Selectable( curveBuf, false );
						ImGui::PopStyleColor();
						if( ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) )
						{
							m_meshDetailsState.linkedCurveIndex = (int)channel.curveIndex;
							m_meshDetailsState.navigateToLinkedCurve = true;
						}
						ImGui::PopID();
						break;
					}
					}
				}
			}
		}
		clipper.End();
		ImGui::EndTable();
	}
}

void UIRenderer::RenderAnimationCurvesSubTab( const cmf::Animation& anim )
{
	int scrollToCurve = -1;
	if( m_meshDetailsState.navigateToLinkedCurve )
	{
		scrollToCurve = m_meshDetailsState.linkedCurveIndex;
		m_meshDetailsState.navigateToLinkedCurve = false;
	}

	if( anim.curves.empty() )
	{
		ImGui::Text( "No curves" );
		return;
	}

	static const char* allCurveCols[] = { "Interpolation", "Knot Type", "Value Type", "Dimensions", "Knots" };
	for( const auto& col : allCurveCols )
		if( m_meshDetailsState.curveColumnFilter.find( col ) == m_meshDetailsState.curveColumnFilter.end() )
			m_meshDetailsState.curveColumnFilter[col] = true;

	if( ImGui::CollapsingHeader( "Filters" ) )
	{
		if( ImGui::Button( "Reset##cvf" ) )
			for( auto& [name, enabled] : m_meshDetailsState.curveColumnFilter )
				enabled = true;
		for( const auto& col : allCurveCols )
		{
			bool enabled = m_meshDetailsState.curveColumnFilter[col];
			if( ImGui::Checkbox( col, &enabled ) )
				m_meshDetailsState.curveColumnFilter[col] = enabled;
		}
	}

	std::vector<CurveColumn> activeCurveCols;
	for( int i = 0; i < 5; ++i )
		if( m_meshDetailsState.curveColumnFilter[allCurveCols[i]] )
			activeCurveCols.push_back( static_cast<CurveColumn>( i ) );

	ImGuiTableFlags tableFlags =
		ImGuiTableFlags_Borders |
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_ScrollX |
		ImGuiTableFlags_ScrollY |
		ImGuiTableFlags_SizingFixedFit;

	int colCount = (int)activeCurveCols.size() + 1;
	if( ImGui::BeginTable( "##curvestable", colCount, tableFlags, ImVec2( 0.0f, ImGui::GetContentRegionAvail().y ) ) )
	{
		if( scrollToCurve >= 0 )
			ImGui::SetScrollY( (float)scrollToCurve * ImGui::GetTextLineHeightWithSpacing() );

		ImGui::TableSetupScrollFreeze( 1, 1 );
		ImGui::TableSetupColumn( "Index", ImGuiTableColumnFlags_WidthFixed, 48.0f );
		for( CurveColumn ci : activeCurveCols )
		{
			bool isLast = ( ci == activeCurveCols.back() );
			ImGui::TableSetupColumn( allCurveCols[(int)ci], isLast ? ImGuiTableColumnFlags_WidthStretch : ImGuiTableColumnFlags_WidthFixed, 80.0f );
		}
		ImGui::TableHeadersRow();

		ImGuiListClipper clipper;
		clipper.Begin( (int)anim.curves.size() );
		while( clipper.Step() )
		{
			for( int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i )
			{
				const auto& curve = anim.curves[i];

				const char* interp = curve.interpolation == cmf::Interpolation::Linear ? "Linear" : "Step";
				const char* knotTypeName = GetElementTypeName( curve.knotType );
				const char* valueTypeName = GetElementTypeName( curve.valueType );

				ImGui::TableNextRow();
				if( i == m_meshDetailsState.linkedCurveIndex )
					ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32( ImGuiCol_Header ) );

				ImGui::TableSetColumnIndex( 0 );
				ImGui::Text( "%d", i );

				for( int col = 0; col < (int)activeCurveCols.size(); ++col )
				{
					ImGui::TableSetColumnIndex( col + 1 );
					switch( activeCurveCols[col] )
					{
					case CurveColumn::Interpolation:
						ImGui::TextUnformatted( interp );
						break;
					case CurveColumn::KnotType:
						ImGui::TextUnformatted( knotTypeName );
						break;
					case CurveColumn::ValueType:
						ImGui::TextUnformatted( valueTypeName );
						break;
					case CurveColumn::Dimensions:
						ImGui::Text( "%u", (uint32_t)curve.valueDimension );
						break;
					case CurveColumn::Knots:
						ImGui::Text( "%u", curve.knotCount );
						break;
					}
				}
			}
		}
		clipper.End();
		ImGui::EndTable();
	}
}

void UIRenderer::RenderAudioOccluderTab( const cmf::Mesh& mesh )
{
	const auto& audioOcclusionMesh = mesh.audioOcclusionMesh;
	if( audioOcclusionMesh.vertices.empty() && audioOcclusionMesh.indices.empty() )
	{
		ImGui::Text( "No audio occlusion mesh" );
		return;
	}

	uint32_t vertexCount = (uint32_t)audioOcclusionMesh.vertices.size();
	uint32_t indexCount = (uint32_t)audioOcclusionMesh.indices.size();
	uint32_t triCount = indexCount / 3;

	ImGui::Text( "Vertices: %u  Triangles: %u", vertexCount, triCount );
	const auto& b = audioOcclusionMesh.bounds;
	auto sz = b.Size();
	ImGui::Text( "Bounds Min:  %.4f  %.4f  %.4f", b.m_min.x, b.m_min.y, b.m_min.z );
	ImGui::Text( "Bounds Max:  %.4f  %.4f  %.4f", b.m_max.x, b.m_max.y, b.m_max.z );
	ImGui::Text( "Bounds Size: %.4f  %.4f  %.4f", sz.x, sz.y, sz.z );

	ImGui::Spacing();

	if( ImGui::CollapsingHeader( "Vertices", ImGuiTreeNodeFlags_DefaultOpen ) )
	{
		static const char* allVertCols[] = { "X", "Y", "Z" };
		for( const auto& col : allVertCols )
			if( m_meshDetailsState.audioVertexColumnFilter.find( col ) == m_meshDetailsState.audioVertexColumnFilter.end() )
				m_meshDetailsState.audioVertexColumnFilter[col] = true;

		if( ImGui::CollapsingHeader( "Filters##avf" ) )
		{
			if( ImGui::Button( "Reset##avr" ) )
				for( auto& [name, enabled] : m_meshDetailsState.audioVertexColumnFilter )
					enabled = true;
			for( const auto& col : allVertCols )
			{
				bool enabled = m_meshDetailsState.audioVertexColumnFilter[col];
				if( ImGui::Checkbox( col, &enabled ) )
					m_meshDetailsState.audioVertexColumnFilter[col] = enabled;
			}
		}

		std::vector<AudioVertexColumn> activeVertCols;
		for( int i = 0; i < 3; ++i )
			if( m_meshDetailsState.audioVertexColumnFilter[allVertCols[i]] )
				activeVertCols.push_back( static_cast<AudioVertexColumn>( i ) );

		ImGuiTableFlags tableFlags =
			ImGuiTableFlags_Borders |
			ImGuiTableFlags_RowBg |
			ImGuiTableFlags_ScrollX |
			ImGuiTableFlags_ScrollY |
			ImGuiTableFlags_SizingFixedFit;

		int colCount = (int)activeVertCols.size() + 1;
		float tableHeight = std::min( (float)vertexCount * ImGui::GetTextLineHeightWithSpacing() + ImGui::GetTextLineHeightWithSpacing(), 200.0f );
		if( ImGui::BeginTable( "##aomverts", colCount, tableFlags, ImVec2( 0.0f, tableHeight ) ) )
		{
			ImGui::TableSetupScrollFreeze( 1, 1 );
			ImGui::TableSetupColumn( "Index", ImGuiTableColumnFlags_WidthFixed, 48.0f );
			for( AudioVertexColumn ci : activeVertCols )
			{
				bool isLast = ( ci == activeVertCols.back() );
				ImGui::TableSetupColumn( allVertCols[(int)ci], isLast ? ImGuiTableColumnFlags_WidthStretch : ImGuiTableColumnFlags_WidthFixed, 80.0f );
			}
			ImGui::TableHeadersRow();

			ImGuiListClipper clipper;
			clipper.Begin( (int)vertexCount );
			while( clipper.Step() )
			{
				for( int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i )
				{
					const auto& v = audioOcclusionMesh.vertices[i];
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex( 0 );
					ImGui::Text( "%d", i );
					for( int col = 0; col < (int)activeVertCols.size(); ++col )
					{
						ImGui::TableSetColumnIndex( col + 1 );
						switch( activeVertCols[col] )
						{
						case AudioVertexColumn::X:
							ImGui::Text( "%.4f", v.x );
							break;
						case AudioVertexColumn::Y:
							ImGui::Text( "%.4f", v.y );
							break;
						case AudioVertexColumn::Z:
							ImGui::Text( "%.4f", v.z );
							break;
						}
					}
				}
			}
			clipper.End();
			ImGui::EndTable();
		}
	}

	ImGui::Spacing();

	if( triCount > 0 && ImGui::CollapsingHeader( "Triangles", ImGuiTreeNodeFlags_DefaultOpen ) )
	{
		ImGuiTableFlags tableFlags =
			ImGuiTableFlags_Borders |
			ImGuiTableFlags_RowBg |
			ImGuiTableFlags_ScrollX |
			ImGuiTableFlags_ScrollY |
			ImGuiTableFlags_SizingFixedFit;

		float tableHeight = std::min( (float)triCount * ImGui::GetTextLineHeightWithSpacing() + ImGui::GetTextLineHeightWithSpacing(), 200.0f );
		if( ImGui::BeginTable( "##aomtris", 4, tableFlags, ImVec2( 0.0f, tableHeight ) ) )
		{
			ImGui::TableSetupScrollFreeze( 1, 1 );
			ImGui::TableSetupColumn( "Triangle", ImGuiTableColumnFlags_WidthFixed, 60.0f );
			ImGui::TableSetupColumn( "V0", ImGuiTableColumnFlags_WidthFixed, 48.0f );
			ImGui::TableSetupColumn( "V1", ImGuiTableColumnFlags_WidthFixed, 48.0f );
			ImGui::TableSetupColumn( "V2", ImGuiTableColumnFlags_WidthStretch );
			ImGui::TableHeadersRow();

			ImGuiListClipper clipper;
			clipper.Begin( (int)triCount );
			while( clipper.Step() )
			{
				for( int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i )
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex( 0 );
					ImGui::Text( "%d", i );
					ImGui::TableSetColumnIndex( 1 );
					ImGui::Text( "%u", (uint32_t)audioOcclusionMesh.indices[i * 3 + 0] );
					ImGui::TableSetColumnIndex( 2 );
					ImGui::Text( "%u", (uint32_t)audioOcclusionMesh.indices[i * 3 + 1] );
					ImGui::TableSetColumnIndex( 3 );
					ImGui::Text( "%u", (uint32_t)audioOcclusionMesh.indices[i * 3 + 2] );
				}
			}
			clipper.End();
			ImGui::EndTable();
		}
	}
}

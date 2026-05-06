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

	state.cmfContent.RegisterCallback( [this]( CmfContent* content, AppState& appState ) {
		if( content == nullptr )
		{
			m_loadStatus = LoadStatus::FAILED;
		}
		else
		{
			m_loadStatus = LoadStatus::SUCCESSFUL;
		}
	} );

	state.currentAnimation.RegisterCallback( [this]( std::string animationName, AppState& appState ) {
		m_playback.currentTime = 0.0f;
		m_playback.playing = false;

		if( animationName != "" && appState.cmfContent.GetValue() != nullptr )
		{
			for( const auto& animation : appState.cmfContent.GetValue()->m_cmfData->animations )
			{
				if( strcmp( animation.name.data(), animationName.data() ) == 0 )
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

	auto [width, height] = state.windowSize.GetValue();
	m_graphicsCommandBuffer.SetRenderSize( width, height );
	UpdateUiState( state );
}

void UIRenderer::FileOpenDialog( AppState& appState )
{
	char const* filter[1] = { "*.cmf" };
	const char* path = tinyfd_openFileDialog(
		"Open CMF File",
		NULL,
		1,
		filter,
		"CMF Files",
		0 );

	if( path != nullptr )
	{
		appState.cmfContent.ForceSetValue( CmfContentLoader::LoadContentFromFile( path ) );
		appState.cmfPath.SetValue( std::string( path ) );
	}
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

	float ySize = height - MENU_BAR_HEIGHT - ANIMATION_PLAYER_HEIGHT + 1; // +1 so we get an  overlap of the borders

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

	float ySize = height - MENU_BAR_HEIGHT - ANIMATION_PLAYER_HEIGHT + 1; // +1 so we get an overlap of the borders

	// Pivot (1,0) anchors the top-right corner to the right edge of the viewport
	ImGui::SetNextWindowPos( ImVec2( width, MENU_BAR_HEIGHT ), ImGuiCond_Always, ImVec2( 1.0f, 0.0f ) );
	ImGui::SetNextWindowSizeConstraints( ImVec2( 0, ySize ), ImVec2( width, ySize ) );
	ImGui::SetNextWindowSize( ImVec2( width / 4.0f, ySize ), ImGuiCond_FirstUseEver );
	if( !ImGui::Begin( "Mesh Details", &open, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings ) )
	{
		ImGui::End();
		return;
	}

	auto* cmfContent = appState.cmfContent.GetValue();
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
		if( ImGui::BeginTabItem( "Morph Data" ) )
		{
			RenderMorphDataTab( cmfContent, mesh, lod );
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
		SetupCombo( "##polygonmode", m_uiState.polygonModeComboBox, appState.polygonMode );
		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		ImGui::Text( "Visualization" );
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x );
		SetupCombo( "##visualiationMode", m_uiState.visualizationShaderComboBox, appState.visualizationShader );
		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		ImGui::Text( "Bounding Box" );
		ImGui::TableNextColumn();
		bool boundingBox = m_uiState.modelStates.boundingBox;
		OnChange( ImGui::Checkbox( "##boundingboxcheckbox", &boundingBox ), [&appState, &boundingBox]() {
			appState.modelBoundingBox.SetValue( boundingBox );
		} );
		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		ImGui::Text( "Wireframe Overlay" );
		ImGui::TableNextColumn();
		bool wireframeOverlay = std::all_of( appState.meshWireframeOverlay.begin(), appState.meshWireframeOverlay.end(), []( const State<bool>& state ) {
									return state.GetValue();
								} ) &&
			appState.meshWireframeOverlay.size() > 0;
		OnChange( ImGui::Checkbox( "##wireframecheckbox", &wireframeOverlay ), [&appState, &wireframeOverlay]() {
			std::for_each( appState.meshWireframeOverlay.begin(), appState.meshWireframeOverlay.end(), [wireframeOverlay]( State<bool>& state ) {
				state.SetValue( wireframeOverlay );
			} );
		} );
		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		ImGui::Text( "Audio Occlusion Mesh" );
		ImGui::TableNextColumn();
		bool audioOcclusion = std::all_of( appState.audioOcclusionMesh.begin(), appState.audioOcclusionMesh.end(), []( const State<bool>& state ) {
								  return state.GetValue();
							  } ) &&
			appState.audioOcclusionMesh.size() > 0;
		OnChange( ImGui::Checkbox( "##audioocclusioncheckbox", &audioOcclusion ), [&appState, &audioOcclusion]() {
			std::for_each( appState.audioOcclusionMesh.begin(), appState.audioOcclusionMesh.end(), [audioOcclusion]( State<bool>& state ) {
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
			SetupCombo( "##selectedlod", m_uiState.modelStates.lod, appState.selectedLod );

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
				appState.meshVisibilityStates[mesh.meshIndex].SetValue( display );
			} );

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text( "Bounding Box" );
			ImGui::SetItemTooltip( "Toggles the bounding box for \"%s\" mesh", mesh.name.c_str() );

			ImGui::TableNextColumn();
			bool boundingBox = mesh.boundingBox;
			OnChange( ImGui::Checkbox( "##boundingboxcheckbox", &boundingBox ), [&appState, &mesh, &boundingBox]() {
				appState.meshBoundingBox[mesh.meshIndex].SetValue( boundingBox );
			} );
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text( "Wireframe Overlay" );
			ImGui::SetItemTooltip( "Toggles the wireframe overlay for the \"%s\" mesh", mesh.name.c_str() );

			ImGui::TableNextColumn();
			bool wireframeOverlay = mesh.wireframeOverlay;
			OnChange( ImGui::Checkbox( "##wireframeoverlaycheckbox", &wireframeOverlay ), [&appState, &mesh, &wireframeOverlay]() {
				appState.meshWireframeOverlay[mesh.meshIndex].SetValue( wireframeOverlay );
			} );
			ImGui::TableNextRow();

			ImGui::BeginDisabled( !mesh.hasAudioOcclusionMesh );
			ImGui::TableNextColumn();
			ImGui::Text( "Audio Occlusion Mesh" );
			ImGui::SetItemTooltip( "Toggles the audio occlusion mesh rendering for the \"%s\" mesh", mesh.name.c_str() );
			ImGui::TableNextColumn();
			bool audioOcclusion = mesh.audioOcclusionMesh;
			OnChange( ImGui::Checkbox( "##audioocclusionmeshcheckbox", &audioOcclusion ), [&appState, &mesh, &audioOcclusion]() {
				appState.audioOcclusionMesh[mesh.meshIndex].SetValue( audioOcclusion );
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
			appState.morphTargetWeight[morphTarget.index].SetValue( weight );
		} );
		ImGui::TableNextColumn();
		bool enabled = morphTarget.enabled;

		OnChange( ImGui::Checkbox( "##checkbox", &enabled ), [&appState, &morphTarget, &enabled]() {
			appState.morphTargetEnabled[morphTarget.index].SetValue( enabled );
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
				FileOpenDialog( appState );
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

void UIRenderer::SetupPlaybackControls( AppState& appState )
{
	float width = (float)appState.windowSize.GetValue().first;
	float height = (float)appState.windowSize.GetValue().second;

	// update the current time
	if( m_playback.playing )
	{
		m_playback.currentTime += ImGui::GetIO().DeltaTime;
		appState.currentAnimationTime.SetValue( m_playback.currentTime );
		if( m_playback.currentTime >= m_playback.duration )
		{
			m_playback.currentTime = m_playback.duration;
			m_playback.playing = false;
		}
	}

	// animation player
	ImGui::SetNextWindowPos( ImVec2( 0, height - ANIMATION_PLAYER_HEIGHT ), ImGuiCond_Always );
	ImGui::SetNextWindowSize( ImVec2( width, ANIMATION_PLAYER_HEIGHT ), ImGuiCond_Always );
	bool open = true;
	if( ImGui::Begin( "Animation", &open, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize ) )
	{
		bool disabled = m_playback.animationComboBox.selectedItemValue == "";
		if( disabled )
		{
			ImGui::BeginDisabled();
		}
		OnChange( ImGui::Button( GetPlaybackButtonLabel(), ImVec2( 64.0f, 18.0f ) ), [this]() { HandlePlaybackButtonPressed(); } );

		ImGui::SameLine();
		float availableWidth = ImGui::GetContentRegionAvail().x;
		ImGui::PushItemWidth( availableWidth - 128.0f );
		ImGui::SliderFloat( "##playbackSlider", &m_playback.currentTime, 0.0f, m_playback.duration );
		ImGui::SetItemTooltip( "%.3fs / %.3fs", m_playback.currentTime, m_playback.duration );

		if( disabled )
		{
			ImGui::EndDisabled();
		}
		ImGui::SameLine();

		ImGui::PushItemWidth( 120.0f );
		// selection box to the right
		SetupCombo( "##animation", m_playback.animationComboBox, appState.currentAnimation );
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
		FileOpenDialog( appState );
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
		appState.selectedLod.SetValue( 0 );
		appState.currentAnimation.SetValue( "" );
		appState.currentAnimationTime.SetValue( 0.0f );
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
		m_uiState.polygonModeComboBox.SetSelectedItemByValue( appState.polygonMode.GetValue() );

		size_t meshIndex = 0;
		size_t morphIndex = 0;
		size_t maxLod = 0;
		for( const auto& mesh : cmfContent->m_cmfData->meshes )
		{
			if( meshIndex >= appState.meshVisibilityStates.size() )
			{
				break;
			}
			MeshUiState meshState{};
			meshState.meshIndex = meshIndex;
			meshState.name = cmf::ToStdString( mesh.name );
			meshState.lodCount = static_cast<uint32_t>( mesh.lods.size() );
			meshState.display = appState.meshVisibilityStates[meshIndex].GetValue();
			meshState.wireframeOverlay = appState.meshWireframeOverlay[meshIndex].GetValue();
			meshState.audioOcclusionMesh = appState.audioOcclusionMesh[meshIndex].GetValue();
			meshState.hasAudioOcclusionMesh = !mesh.audioOcclusionMesh.vertices.empty() && !mesh.audioOcclusionMesh.indices.empty();
			meshState.boundingBox = appState.meshBoundingBox[meshIndex].GetValue();

			maxLod = std::max( maxLod, mesh.lods.size() );

			for( const auto& lod : mesh.lods )
			{
				meshState.vertexCount += lod.vb.size / lod.vb.stride;
				meshState.indexCount += lod.ib.size / lod.ib.stride;
			}

			m_uiState.modelStates.totalVertexCount += meshState.vertexCount;
			m_uiState.modelStates.totalIndexCount += meshState.indexCount;

			for( const auto& morphTarget : mesh.morphTargets.targets )
			{
				if( morphIndex >= appState.morphTargetWeight.size() )
				{
					break;
				}
				MorphTargetUiState morphTargetState{};
				morphTargetState.name = cmf::ToStdString( morphTarget.name );
				morphTargetState.weight = appState.morphTargetWeight[morphIndex].GetValue();
				morphTargetState.enabled = appState.morphTargetEnabled[morphIndex].GetValue();
				morphTargetState.index = morphIndex++;
				meshState.morphTargets.push_back( morphTargetState );
			}

			m_uiState.modelStates.meshes.push_back( meshState );
			meshIndex++;
		}

		// visualization shader selection
		for( const auto& shaderName : appState.availableShaders.GetValue() )
		{
			m_uiState.visualizationShaderComboBox.items.push_back( { shaderName, shaderName } );
		}
		m_uiState.visualizationShaderComboBox.SetSelectedItemByValue( appState.visualizationShader.GetValue() );

		for( uint32_t lod = 0; lod < maxLod; ++lod )
		{
			m_uiState.modelStates.lod.items.push_back( std::make_pair( "Lod " + std::to_string( lod ), lod ) );
		}

		m_uiState.modelStates.lod.SetSelectedItemByValue( appState.selectedLod.GetValue() );
		m_uiState.modelStates.boundingBox = appState.modelBoundingBox.GetValue();

		if( m_playback.animationComboBox.items.empty() )
		{
			m_playback.animationComboBox.items.push_back( std::make_pair( "No Animation", "" ) );
			for( const auto& animation : cmfContent->m_cmfData->animations )
			{
				auto animationName = cmf::ToStdString( animation.name );
				m_playback.animationComboBox.items.push_back( std::make_pair( animationName, animationName ) );
			}
			m_playback.animationComboBox.SetSelectedItemByValue( appState.currentAnimation.GetValue() );
		}
	}
	else
	{
		// default uninitialized values
		if( m_playback.animationComboBox.items.empty() )
		{
			m_playback.animationComboBox.items.push_back( std::make_pair( "No Animation", "" ) );
			m_playback.animationComboBox.SetSelectedItemByValue( appState.currentAnimation.GetValue() );
		}

		// polygon mode selection
		m_uiState.polygonModeComboBox.items = {
			{ "Fill", VK_POLYGON_MODE_FILL },
			{ "Line", VK_POLYGON_MODE_LINE },
			{ "Point", VK_POLYGON_MODE_POINT }
		};
		m_uiState.polygonModeComboBox.SetSelectedItemByValue( appState.polygonMode.GetValue() );
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
	case cmf::Usage::Position:            name = "Position"; break;
	case cmf::Usage::Normal:              name = "Normal"; break;
	case cmf::Usage::Tangent:             name = "Tangent"; break;
	case cmf::Usage::Binormal:            name = "Binormal"; break;
	case cmf::Usage::TexCoord:            name = "TexCoord"; break;
	case cmf::Usage::Color:               name = "Color"; break;
	case cmf::Usage::BoneIndices:         name = "Bone Indices"; break;
	case cmf::Usage::BoneWeights:         name = "Bone Weights"; break;
	case cmf::Usage::PackedTangent:       name = "Packed Tangent"; break;
	case cmf::Usage::PackedTangentLegacy: name = "Packed Tangent (Legacy)"; break;
	default:                              name = "Unknown"; break;
	}
	if( usageIndex > 0 )
		name += std::to_string( usageIndex );
	return name;
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
		ImGuiTableFlags_ScrollY |
		ImGuiTableFlags_SizingFixedFit;

	int totalRows = (int)vertexCount * (int)attributes.size();
	ImVec2 outerSize( 0.0f, ImGui::GetContentRegionAvail().y );
	if( ImGui::BeginTable( tableId, 3, tableFlags, outerSize ) )
	{
		if( scrollToVertex >= 0 )
		{
			float targetY = (float)( scrollToVertex * (int)attributes.size() ) * ImGui::GetTextLineHeightWithSpacing();
			ImGui::SetScrollY( targetY );
		}

		ImGui::TableSetupScrollFreeze( 0, 1 );
		ImGui::TableSetupColumn( "Index", ImGuiTableColumnFlags_WidthFixed, 48.0f );
		ImGui::TableSetupColumn( "Attribute", ImGuiTableColumnFlags_WidthFixed, 150.0f );
		ImGui::TableSetupColumn( "Value", ImGuiTableColumnFlags_WidthStretch );
		ImGui::TableHeadersRow();

		ImGuiListClipper clipper;
		clipper.Begin( totalRows );
		while( clipper.Step() )
		{
			for( int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row )
			{
				int vi = row / (int)attributes.size();
				int ai = row % (int)attributes.size();
				const auto& attr = attributes[ai];
				const uint8_t* vertexPtr = vbData + (size_t)vi * stride;

				ImGui::TableNextRow();
				ImGuiCol bgCol = ( vi == m_meshDetailsState.linkedVertexIndex )
					? ImGuiCol_Header
					: ( vi % 2 == 0 ? ImGuiCol_TableRowBg : ImGuiCol_TableRowBgAlt );
				ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32( bgCol ) );

				ImGui::TableSetColumnIndex( 0 );
				if( ai == 0 )
					ImGui::Text( "%d", vi );

				ImGui::TableSetColumnIndex( 1 );
				ImGui::TextUnformatted( attr.name.c_str() );

				ImGui::TableSetColumnIndex( 2 );
				for( uint8_t c = 0; c < attr.elementCount; ++c )
				{
					const uint8_t* compPtr = vertexPtr + attr.byteOffset + c * attr.conv.second;
					ImGui::Text( "%f", attr.conv.first.to( compPtr ) );
					if( c + 1 < attr.elementCount )
					{
						ImGui::SameLine();
					}
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

    // Get the lables for the vertex atributes
	auto allAttributes = BuildAttributes( mesh.decl );

	for( const auto& attr : allAttributes )
	{
		if( m_meshDetailsState.vertexAttributeFilter.find( attr.name ) == m_meshDetailsState.vertexAttributeFilter.end() )
			m_meshDetailsState.vertexAttributeFilter[attr.name] = true;
	}

    // Vertex atribute filter list
	if( ImGui::CollapsingHeader( "Attribute Filters" ) )
	{
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

	auto indexSelectable = [&]( uint32_t vertexIdx, int uniqueId )
	{
		char buf[16];
		snprintf( buf, sizeof( buf ), "%u", vertexIdx );
		ImGui::PushID( uniqueId );
		bool isSelected = ( (int)vertexIdx == m_meshDetailsState.selectedIndexValue );
		ImGui::Selectable( buf, isSelected, ImGuiSelectableFlags_None );
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

	if( ImGui::CollapsingHeader( "Attribute Filters" ) )
	{
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

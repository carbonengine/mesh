#include "uiRenderer.h"

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

	SetupPlaybackControls( appState );

	SetupPopupWindows( appState );
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
				meshState.vertexCount += lod.vb.size / lod.vb.stride;
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

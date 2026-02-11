#include "uiRenderer.h"

#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <tinyfiledialogs/tinyfiledialogs.h>

#include "appState.h"
#include "vulkan/shadercache.h"
#include "vulkan/vulkanerrors.h"


UIRenderer::UIRenderer( std::shared_ptr<const Renderer> renderer ) :
	m_renderer( renderer ),
	m_commandBuffer( renderer.get() )
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
		m_commandBuffer.SetRenderSize( width, height );
	} );

	auto [width, height] = state.windowSize.GetValue();
	m_commandBuffer.SetRenderSize( width, height );
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
		appState.cmfContent.SetValue( CmfContentLoader::LoadContentFromFile( path ) );
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
	m_commandBuffer.Begin( m_renderer.get() );
	ImGui::Render();

	ImGui_ImplVulkan_RenderDrawData( ImGui::GetDrawData(), m_renderer->GetCurrentVkCommandBuffer() );
	m_commandBuffer.End();
}

void UIRenderer::SetupUi( AppState& appState )
{
	UpdateUiState( appState );
	auto cmfContent = appState.cmfContent.GetValue();
	bool open = true;
	if( ImGui::Begin( "CMF Info", &open, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar ) )
	{
		ImGui::SetWindowPos( ImVec2( 0, 18 ), ImGuiCond_Always );
		SetupGeneralView( appState );

		SetupMeshListView( m_uiState.modelStates, appState );

		ImGui::End();
	}
}

void UIRenderer::SetupGeneralView( AppState& appState )
{
	bool open = true;
	ImGui::SeparatorText( "General" );
	if( ImGui::BeginTable( "##table", 2 ) )
	{
		ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_WidthFixed );
		ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_WidthStretch );
		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		ImGui::Text( "Path" );
		ImGui::TableNextColumn();
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
		SetupCombo( "##polygonmode", m_uiState.polygonModeComboBox, appState.polygonMode );
		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		ImGui::Text( "Visualization" );
		ImGui::TableNextColumn();
		SetupCombo( "##visualiationMode", m_uiState.visualizationShaderComboBox, appState.visualizationShader );
		ImGui::TableNextRow();

        ImGui::TableNextColumn();
		ImGui::Text( "Wireframe Overlay" );
		ImGui::TableNextColumn();
		bool wireframeOverlay = std::all_of( appState.meshWireframeOverlay.begin(), appState.meshWireframeOverlay.end(), []( const State<bool>& state ) {
            return state.GetValue();
		} ) && appState.meshWireframeOverlay.size() > 0;
		OnChange( ImGui::Checkbox( "##checkbox", &wireframeOverlay ), [&appState, &wireframeOverlay]() {
			std::for_each(appState.meshWireframeOverlay.begin(), appState.meshWireframeOverlay.end(), [wireframeOverlay]( State<bool>& state ) {
                state.SetValue( wireframeOverlay );
            });
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
			ImGui::Text( "LOD Count" );
			ImGui::TableNextColumn();
			ImGui::Text( "%d", mesh.lodCount );

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
			ImGui::Text( "Wireframe Overlay" );
			ImGui::SetItemTooltip( "Toggles the wireframe overlay for the \"%s\" mesh", mesh.name.c_str() );

			ImGui::TableNextColumn();
			bool wireframeOverlay = mesh.wireframeOverlay;
			OnChange( ImGui::Checkbox( "##wireframeoverlaycheckbox", &wireframeOverlay ), [&appState, &mesh, &wireframeOverlay]() {
				appState.meshWireframeOverlay[mesh.meshIndex].SetValue( wireframeOverlay );
			} );
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
		ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_WidthFixed );
		ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_WidthStretch );
		ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_WidthFixed, 30.0 );
		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		ImGui::Text( "Weight" );
		ImGui::TableNextColumn();
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

	if( m_cmfFullReset )
	{
		// reset all appStae items related to cmf
		appState.selectedLod.SetValue( 0 );
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

		// visualization shader selection
		for( const auto& shaderName : ShaderCache::GetAvailableShaderNames() )
		{
			m_uiState.visualizationShaderComboBox.items.push_back( { shaderName, shaderName } );
		}
		m_uiState.visualizationShaderComboBox.SetSelectedItemByValue( appState.visualizationShader.GetValue() );

		size_t meshIndex = 0;
		size_t morphIndex = 0;
		for( const auto& mesh : cmfContent->m_cmfData->meshes )
		{
			if( meshIndex >= appState.meshVisibilityStates.size() )
            {
				break;
			}
			MeshUiState meshState{};
			meshState.meshIndex = meshIndex;
			meshState.name = mesh.name.data();
			meshState.lodCount = static_cast<uint32_t>( mesh.lods.size() );
			meshState.display = appState.meshVisibilityStates[meshIndex].GetValue();
			meshState.wireframeOverlay = appState.meshWireframeOverlay[meshIndex].GetValue();

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
				morphTargetState.name = morphTarget.name.data();
				morphTargetState.weight = appState.morphTargetWeight[morphIndex].GetValue();
				morphTargetState.enabled = appState.morphTargetEnabled[morphIndex].GetValue();
				morphTargetState.index = morphIndex++;
				meshState.morphTargets.push_back( morphTargetState );
			}

			m_uiState.modelStates.meshes.push_back( meshState );
			meshIndex++;
		}
		m_uiState.modelStates.selectedLod = appState.selectedLod.GetValue();
	}

	m_cmfFullReset = false;
}

void UIRenderer::OnChange( bool changed, std::function<void()> callback )
{
	if( changed )
	{
		callback();
	}
}

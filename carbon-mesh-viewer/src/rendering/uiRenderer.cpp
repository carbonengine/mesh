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

	state.cmfContent.RegisterCallback( [this, &state]( CmfContent* content, const AppState& ) { m_cmfFullReset = true; } );
	state.selectedMesh.RegisterCallback( [this, &state]( int32_t content, const AppState& ) { m_cmfAttributeChange = true; } );
	state.selectedLod.RegisterCallback( [this, &state]( uint32_t content, const AppState& ) { m_cmfAttributeChange = true; } );
	state.windowSize.RegisterCallback( [this]( std::pair<uint32_t, uint32_t> size, const AppState& appstate ) {
		auto [width, height] = size;
		m_commandBuffer.SetRenderSize( width, height );
	} );

	auto [width, height] = state.windowSize.GetValue();
	m_commandBuffer.SetRenderSize( width, height );
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
	if( appState.cmfContent.GetValue() )
	{
		SetupUi( appState );
	}
	m_commandBuffer.Begin( m_renderer.get() );
	ImGui::Render();

	ImGui_ImplVulkan_RenderDrawData( ImGui::GetDrawData(), m_renderer->GetCurrentVkCommandBuffer() );
	m_commandBuffer.End();
}

void UIRenderer::SetupUi( AppState& appState )
{
	UpdateUiState( appState );
	auto cmfContent = appState.cmfContent.GetValue();
	if( cmfContent != nullptr )
	{
		if( ImGui::Begin( "CMF Info" ) )
		{
			ImGui::InputText( "Path", m_uiState.filePath.data(), m_uiState.filePath.size(), ImGuiInputTextFlags_ElideLeft | ImGuiInputTextFlags_ReadOnly );

			SetupCombo( "Mesh", m_uiState.meshComboBox, appState.selectedMesh );
			SetupCombo( "Lod", m_uiState.lodComboBox, appState.selectedLod );
			SetupCombo( "Polygon Mode", m_uiState.polygonModeComboBox, appState.polygonMode );
			SetupCombo( "Visualization", m_uiState.visualizationShaderComboBox, appState.visualizationShader );

			ImGui::LabelText( "Vertices", "%d", m_uiState.vertexCount );
			ImGui::LabelText( "Indices", "%d", m_uiState.indexCount );
		}
		ImGui::End();
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
	if( !m_cmfFullReset && !m_cmfAttributeChange || !appState.cmfContent.GetValue() )
	{
		return;
	}
	if( m_cmfFullReset )
	{
		appState.selectedLod.SetValue( 0 );
		appState.selectedMesh.SetValue( -1 );
	}

	int32_t selectedLod = appState.selectedLod.GetValue();
	uint32_t selectedMesh = appState.selectedMesh.GetValue();

	m_uiState = CmfUiState{};

	auto cmfContent = appState.cmfContent.GetValue();
	if( cmfContent != nullptr )
	{
		m_uiState.filePath = appState.cmfPath.GetValue();
		m_uiState.selectedMesh = appState.selectedMesh.GetValue();
		m_uiState.vertexCount = 0;
		m_uiState.indexCount = 0;

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

		int32_t meshIndex = 0;
		uint32_t maxLod = 0;
		uint32_t currentLod = appState.selectedLod.GetValue();
		int32_t currentMesh = appState.selectedMesh.GetValue();

		// add a "All Meshes" option
		m_uiState.meshComboBox.items.push_back( { "All", -1 } );

		for( const auto& mesh : cmfContent->m_cmfData->meshes )
		{
			auto lod = mesh.lods[std::min( currentLod, (uint32_t)mesh.lods.size() - 1 )];
			if( currentMesh == -1 || currentMesh == meshIndex )
			{
				m_uiState.indexCount += lod.ib.size / lod.ib.stride;
				m_uiState.vertexCount += lod.vb.size / lod.vb.stride;

				maxLod = std::max( maxLod, (uint32_t)mesh.lods.size() );
			}
			m_uiState.meshComboBox.items.push_back( { mesh.name.data(), meshIndex++ } );
		}
		m_uiState.meshComboBox.SetSelectedItemByValue( currentMesh );

		for( uint32_t i = 0; i < maxLod; i++ )
		{
			m_uiState.lodComboBox.items.push_back( { "Lod " + std::to_string( i ), i } );
		}
		m_uiState.lodComboBox.SetSelectedItemByValue( currentLod );
	}

	m_cmfAttributeChange = false;
	m_cmfFullReset = false;
}
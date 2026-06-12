#include "uiRenderer.h"

#include <cmf/converters.h>
#include <cmf/bufferstreams.h>
#include <cmf/declutils.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <tinyfiledialogs/tinyfiledialogs.h>

#include "appState.h"
#include "vulkan/vulkanerrors.h"

#include <numeric>
#include <filesystem>

// ImGui is using a lot of variadic functions for text formatting, so we disable the cppcoreguidelines-pro-type-vararg lint for this file
// NOLINTBEGIN(cppcoreguidelines-pro-type-vararg)

const float MENU_BAR_HEIGHT = 18.0f;
const float ANIMATION_PLAYER_HEIGHT = 36.0f;
const float BUTTON_SIZE = 18.0f;

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

		SetupSkeletonOwners( m_uiState.skeletonOwners, appState );
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
	if( !ImGui::Begin( "Details", &open, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings ) )
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

	if( !ImGui::BeginChild( "Hierarchy", ImVec2( 0, 0 ), ImGuiChildFlags_ResizeY ) )
	{
		ImGui::EndChild();
		return;
	}
	auto selection = RenderHierarchyTab( *cmfContent );
	for( auto& animSource : appState.modelState.animationOverrides )
	{
		auto otherSelection = RenderHierarchyTab( *animSource.GetValue() );
		if( otherSelection.type != SelectedItem::None )
		{
			selection = otherSelection;
		}
	}
	ImGui::EndChild();
	
	if( selection.type != SelectedItem::None )
	{
		m_selectedItem = selection;
	}

	auto renderVertexBuffer = [&]( const void* vb, const CmfContent& cmfContent ) {
		for( auto& mesh : cmfContent.m_cmfData->meshes )
		{
			for( auto& lod : mesh.lods )
			{
				if( &lod.vb == vb )
				{
					RenderVertexDataTab( cmfContent, mesh.decl, lod.vb );
					return true;
				}
				for( auto& morphTarget : lod.morphTargets )
				{
					if( &morphTarget.vb == vb )
					{
						RenderVertexDataTab( cmfContent, mesh.morphTargets.decl, morphTarget.vb );
						return true;
					}
				}
			}
		}
		return false;
	};
	auto renderIndexBuffer = [&]( const void* ib, const CmfContent& cmfContent ) {
		for( auto& mesh : cmfContent.m_cmfData->meshes )
		{
			for( auto& lod : mesh.lods )
			{
				if( &lod.ib == ib )
				{
					RenderIndexDataTab( cmfContent, mesh, lod );
					return true;
				}
			}
		}
		return false;
	};
	auto renderSkeleton = [&]( const void* skeletonPtr, const CmfContent& cmfContent ) {
		for( auto& skeleton : cmfContent.m_cmfData->skeletons )
		{
			if( &skeleton == skeletonPtr )
			{
				RenderBonesTab( cmfContent, skeleton, appState );
				return true;
			}
		}
		return false;
	};
	auto renderBoneBindings = [&]( const void* boneBindingsPtr, const CmfContent& cmfContent ) {
		for( auto& mesh : cmfContent.m_cmfData->meshes )
		{
			if( &mesh.boneBindings == boneBindingsPtr )
			{
				RenderBoneBindingTab( cmfContent, mesh, appState );
				return true;
			}
		}
		return false;
	};
	auto renderAnimation = [&]( const void* animationPtr, const CmfContent& cmfContent ) {
		for( auto& animation : cmfContent.m_cmfData->animations )
		{
			if( &animation == animationPtr )
			{
				RenderAnimationChannelsSubTab( animation, *cmfContent.m_cmfData );
				return true;
			}
		}
		return false;
	};
	auto renderAnimationCurve = [&]( const void* curvePtr, const CmfContent& cmfContent ) {
		for( auto& animation : cmfContent.m_cmfData->animations )
		{
			for( auto& curve : animation.curves )
			{
				if( &curve == curvePtr )
				{
					RenderAnimationCurvesSubTab( curve, animation );
					return true;
				}
			}
		}
		return false;
	};
	auto renderAudioOcclusionMesh = [&]( const void* audioOcclusionMeshPtr, const CmfContent& cmfContent ) {
		for( auto& mesh : cmfContent.m_cmfData->meshes )
		{
			if( &mesh.audioOcclusionMesh == audioOcclusionMeshPtr )
			{
				RenderAudioOccluderTab( mesh.audioOcclusionMesh );
				return true;
			}
		}
		return false;
	};

	auto renderForDataSources = [&]( const void* context, const auto& render ) {
		if( !render( context, *cmfContent ) )
		{
			for( const auto& animSource : appState.modelState.animationOverrides )
			{
				if( render( context, *animSource.GetValue() ) )
				{
					return;
				}
			}
		}
	};

	switch( m_selectedItem.type )
	{
	case SelectedItem::VertexBuffer:
		renderForDataSources( m_selectedItem.context, renderVertexBuffer );
		break;
	case SelectedItem::IndexBuffer:
		renderForDataSources( m_selectedItem.context, renderIndexBuffer );
		break;
	case SelectedItem::SkeletonBones:
		renderForDataSources( m_selectedItem.context, renderSkeleton );
		break;
	case SelectedItem::BoneBindings:
		renderForDataSources( m_selectedItem.context, renderBoneBindings );
		break;
	case SelectedItem::Animation:
		renderForDataSources( m_selectedItem.context, renderAnimation );
		break;
	case SelectedItem::AnimationCurve:
		renderForDataSources( m_selectedItem.context, renderAnimationCurve );
		break;
	case SelectedItem::AudioOcclusionMesh:
		renderForDataSources( m_selectedItem.context, renderAudioOcclusionMesh );
		break;
	default:
		break;
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

		bool hasAudioOcclusionMeshes = std::any_of( m_uiState.modelStates.meshes.begin(), m_uiState.modelStates.meshes.end(), []( const MeshUiState& state ) {
			return state.hasAudioOcclusionMesh;
		} );
		ImGui::BeginDisabled( !hasAudioOcclusionMeshes );
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
		ImGui::EndDisabled();

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
		ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_WidthFixed );
		ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_WidthStretch );
		ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_WidthFixed, BUTTON_SIZE );
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
		auto filePath = FileOpenDialog();
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
			if( meshIndex >= appState.modelState.meshVisibilityStates.size() )
			{
				break;
			}
			MeshUiState meshState{};
			meshState.meshIndex = meshIndex;
			meshState.name = cmf::ToStdString( mesh.name );
			meshState.lod = appState.modelState.activeLod[meshIndex].GetValue();
			meshState.maxLodIndex = static_cast<uint32_t>( mesh.lods.size() ) - 1;
			meshState.boundingSphereRadius = CcpMath::Sphere( mesh.bounds ).radius;
			meshState.screenSize = appState.modelState.meshScreenSize[meshIndex].GetValue();
			meshState.display = appState.modelState.meshVisibilityStates[meshIndex].GetValue();
			meshState.wireframeOverlay = appState.modelState.meshWireframeOverlay[meshIndex].GetValue();
			meshState.audioOcclusionMesh = appState.modelState.audioOcclusionMesh[meshIndex].GetValue();
			meshState.hasAudioOcclusionMesh = !mesh.audioOcclusionMesh.vertices.empty() && !mesh.audioOcclusionMesh.indices.empty();
			meshState.boundingBox = appState.modelState.meshBoundingBox[meshIndex].GetValue();

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

void UIRenderer::RenderAttributeTable( const char* tableId, const uint8_t* vbData, uint32_t vertexCount, uint32_t stride, const std::vector<AttributeInfo>& attributes )
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
		if( m_selectedItem.scrollTo && !m_selectedItem.selectedIndices.empty() )
		{
			float targetY = (float)m_selectedItem.selectedIndices.front() * ImGui::GetTextLineHeightWithSpacing();
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
				if( std::find( m_selectedItem.selectedIndices.begin(), m_selectedItem.selectedIndices.end(), uint32_t( vi ) ) != m_selectedItem.selectedIndices.end() )
				{
					ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32( ImGuiCol_Header ) );
				}
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

void UIRenderer::RenderVertexDataTab( const CmfContent& cmfContent, const cmf::Span<cmf::VertexElement>& decl, const cmf::BufferView& vb )
{
	if( decl.empty() )
	{
		ImGui::Text( "No vertex declaration" );
		return;
	}
	if( vb.stride == 0 || vb.size == 0 )
	{
		ImGui::Text( "Empty vertex buffer" );
		return;
	}

	uint32_t vertexCount = vb.size / vb.stride;
	ImGui::Text( "Vertices: %u   Stride: %u bytes", vertexCount, vb.stride );
	const uint8_t* vbData = cmfContent.Index( vb.index, 0 ) + vb.offset;

	// Get the labels for the vertex atributes
	auto allAttributes = BuildAttributes( decl );

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

	RenderAttributeTable( "##vertexdata", vbData, vertexCount, vb.stride, filteredAttributes );
	m_selectedItem.scrollTo = false;
}

void UIRenderer::RenderIndexDataTab( const CmfContent& cmfContent, const cmf::Mesh& mesh, const cmf::MeshLod& lod )
{
	const auto& ib = lod.ib;
	const auto& areas = mesh.areas;
	const auto& lodAreas = lod.areas;

	if( ib.stride == 0 || ib.size == 0 )
	{
		ImGui::Text( "Empty index buffer" );
		return;
	}

	uint32_t indexCount = ib.size / ib.stride;
	const uint8_t* ibData = cmfContent.Index( ib.index, 0 ) + ib.offset;
	cmf::IndexConverter indexConv( ib.stride );

	auto indexSelectable = [&]( uint32_t vertexIdx, int uniqueId ) {
		ImGui::PushID( uniqueId );
		const bool isSelected = std::find( m_selectedItem.selectedIndices.begin(), m_selectedItem.selectedIndices.end(), vertexIdx ) != m_selectedItem.selectedIndices.end();
		if( ImGui::Selectable( "", isSelected, ImGuiSelectableFlags_AllowOverlap ) )
		{
			m_selectedItem.selectedIndices = { vertexIdx };
		}
		ImGui::SameLine();
		if( ImGui::TextLink( std::to_string( vertexIdx ).c_str() ) )
		{
			m_selectedItem = SelectedItem{ SelectedItem::Type::VertexBuffer, &lod.vb, { vertexIdx }, true };
		}
		ImGui::PopID();
	};

	uint32_t triangleCount = indexCount / 3;
	ImGui::Text( "Triangles: %u   Indices: %u", triangleCount, indexCount );

	ImGuiTableFlags tableFlags =
		ImGuiTableFlags_Borders |
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_ScrollY |
		ImGuiTableFlags_SizingFixedFit;

	ImVec2 outerSize( 0.0f, ImGui::GetContentRegionAvail().y );
	if( ImGui::BeginTable( "##indexdata", 5, tableFlags, outerSize ) )
	{
		ImGui::TableSetupScrollFreeze( 0, 1 );
		ImGui::TableSetupColumn( "Triangle", ImGuiTableColumnFlags_WidthFixed, 72.0f );
		ImGui::TableSetupColumn( "V0", ImGuiTableColumnFlags_WidthStretch );
		ImGui::TableSetupColumn( "V1", ImGuiTableColumnFlags_WidthStretch );
		ImGui::TableSetupColumn( "V2", ImGuiTableColumnFlags_WidthStretch );
		if( !areas.empty() )
		{
			ImGui::TableSetupColumn( "Area", ImGuiTableColumnFlags_WidthStretch );
		}
		ImGui::TableHeadersRow();

		ImGuiListClipper clipper;
		clipper.Begin( (int)triangleCount );
		while( clipper.Step() )
		{
			for( int ti = clipper.DisplayStart; ti < clipper.DisplayEnd; ++ti )
			{
				const uint8_t* base = ibData + (size_t)ti * 3 * ib.stride;
				const uint32_t v0 = indexConv( base );
				const uint32_t v1 = indexConv( base + ib.stride );
				const uint32_t v2 = indexConv( base + 2 * ib.stride );

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex( 0 );
				ImGui::Text( "%d", ti );
				ImGui::TableSetColumnIndex( 1 );
				indexSelectable( v0, ti * 3 );
				ImGui::TableSetColumnIndex( 2 );
				indexSelectable( v1, ti * 3 + 1 );
				ImGui::TableSetColumnIndex( 3 );
				indexSelectable( v2, ti * 3 + 2 );
				if( !areas.empty() )
				{
					ImGui::TableSetColumnIndex( 4 );
					for( const auto& area : lodAreas )
					{
						if( ti >= int( area.firstElement ) && ti < int( area.firstElement + area.elementCount ) )
						{
							auto areaIdx = std::distance( lodAreas.begin(), &area );
							ImGui::Text( "[%i] %s", int( areaIdx ), areas[areaIdx].name.empty() ? "Unnamed Area" : ToStdString( areas[areaIdx].name ).c_str() );
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

void UIRenderer::RenderBonesTab( const CmfContent& cmfContent, const cmf::Skeleton& skeleton, AppState& appState )
{
	const auto& skeletons = cmfContent.m_cmfData->skeletons;

	//if( hasSkeleton )
	{
		std::string skelName = cmf::ToStdString( skeleton.name );
		ImGui::Text( "Skeleton: %s  Bones: %u", skelName.c_str(), (uint32_t)skeleton.bones.size() );

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

		int colCount = (int)activeColIndices.size() + 1;
		if( ImGui::BeginTable( "##boneslist", colCount, tableFlags, ImVec2( 0.0f, ImGui::GetContentRegionAvail().y ) ) )
		{
			if( m_selectedItem.scrollTo && !m_selectedItem.selectedIndices.empty() )
			{
				ImGui::SetScrollY( (float)m_selectedItem.selectedIndices.front() * ImGui::GetTextLineHeightWithSpacing() );
				m_selectedItem.scrollTo = false;
			}

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
					ImGui::PushID( bi );
					uint32_t parentIdx = ( (size_t)bi < skeleton.parents.size() ) ? skeleton.parents[bi] : 0xffffffff;
					bool hasTransform = (size_t)bi < skeleton.restTransforms.size();

					ImGui::TableNextRow();

					if( std::find( m_selectedItem.selectedIndices.begin(), m_selectedItem.selectedIndices.end(), (uint32_t)bi ) != m_selectedItem.selectedIndices.end() )
					{
						ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32( ImGuiCol_Header ) );
					}

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
								if( ImGui::TextLink( parentName.c_str() ) )
								{
									m_selectedItem.selectedIndices = { parentIdx };
									m_selectedItem.scrollTo = true;
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

					ImGui::TableSetColumnIndex( 0 );

					auto index = std::find_if( appState.modelState.selectedBones.begin(), appState.modelState.selectedBones.end(), [bi]( State<uint32_t> selected ) {
						return selected.GetValue() == (uint32_t)bi;
					} );

					bool itemIsSelected = index != appState.modelState.selectedBones.end();
					if( ImGui::Selectable( std::to_string( bi ).c_str(), itemIsSelected, ImGuiSelectableFlags_SpanAllColumns ) )
					{
						if( ImGui::GetIO().KeyCtrl )
						{
							if( itemIsSelected )
							{
								appState.modelState.selectedBones.RemoveAt( std::distance( appState.modelState.selectedBones.begin(), index ) );
							}
							else
							{
								appState.modelState.selectedBones.AddState( (uint32_t)bi );
							}
						}
						else
						{
							appState.modelState.selectedBones.Clear();
							appState.modelState.selectedBones.AddState( (uint32_t)bi );
						}
					}

					ImGui::PopID();
				}
			}
			clipper.End();
			ImGui::EndTable();
		}
	}
}

void UIRenderer::RenderBoneBindingTab( const CmfContent& cmfContent, const cmf::Mesh& mesh, AppState& appState )
{
	const auto& skeletons = cmfContent.m_cmfData->skeletons;
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
				const cmf::Skeleton* skeleton = nullptr;
				if( mesh.skeleton != 0xff )
				{
					skeleton = &cmfContent.m_cmfData->skeletons[mesh.skeleton];
				}
				for( int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i )
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex( 0 );
					ImGui::Text( "%d", i );
					ImGui::TableSetColumnIndex( 1 );

					if( skeleton )
					{
						if( ImGui::TextLink( cmf::ToStdString( mesh.boneBindings[i].name ).c_str() ) )
						{
							uint32_t boneIndex = 0xffffffff;
							auto found = std::find( skeleton->bones.begin(), skeleton->bones.end(), mesh.boneBindings[i].name );
							if( found != skeleton->bones.end() )
							{
								boneIndex = static_cast<uint32_t>( std::distance( skeleton->bones.begin(), found ) );
							}
							m_selectedItem = SelectedItem{ SelectedItem::Type::SkeletonBones, skeleton, { boneIndex }, true };
						}
					}
					else
					{
						ImGui::TextUnformatted( cmf::ToStdString( mesh.boneBindings[i].name ).c_str() );
					}

				}
			}
			clipper.End();
			ImGui::EndTable();
		}
	}
}
UIRenderer::SelectedItem UIRenderer::RenderHierarchyTab( const CmfContent& cmfContent )
{
	UIRenderer::SelectedItem selectedItem = {};

	auto clickableNode = [&]( SelectedItem::Type type, const auto& context, const char* label ) {
		ImGui::PushID( &context );
		ImGui::PushStyleColor( ImGuiCol_HeaderHovered, ImVec4( 0.0f, 0.0f, 0.0f, 0.0f ) );
		ImGui::Indent( ImGui::GetStyle().IndentSpacing );
		ImGui::PushStyleVarX( ImGuiStyleVar_ItemSpacing, 0.0f );
		ImGui::Selectable( "", m_selectedItem.type == type && m_selectedItem.context == &context, ImGuiSelectableFlags_AllowOverlap );
		ImGui::SameLine();
		if( ImGui::TextLink( label ) )
		{
			selectedItem = SelectedItem{ type, &context };
		}
		ImGui::Unindent( ImGui::GetStyle().IndentSpacing );
		ImGui::PopStyleVar();
		ImGui::PopStyleColor();
		ImGui::PopID();
	};
	auto textNode = [&]( const char* label, auto... vars ) {
		ImGui::Indent( ImGui::GetStyle().IndentSpacing );
		ImGui::Text( label, vars... );
		ImGui::Unindent( ImGui::GetStyle().IndentSpacing );
	};

	const std::filesystem::path filePath( cmfContent.m_filePath );
	if( ImGui::TreeNodeEx( &cmfContent, ImGuiTreeNodeFlags_DefaultOpen, "%s", filePath.filename().string().c_str() ) )
	{
		const auto& cmfData = *cmfContent.m_cmfData;
		const auto& meshes = cmfContent.m_cmfData->meshes;
		const auto& skeletons = cmfContent.m_cmfData->skeletons;

		auto renderSkeleton = [&]( const cmf::Skeleton& skeleton ) {
			const auto name = "Skeleton[" + std::to_string( std::distance( skeletons.begin(), &skeleton ) ) + "]: " + cmf::ToStdString( skeleton.name ) + " (" + std::to_string( skeleton.bones.size() ) + " bones)";
			clickableNode( SelectedItem::Type::SkeletonBones, skeleton, name.c_str() );
		};

		if( !meshes.empty() && ImGui::TreeNodeEx( &cmfData.meshes, ImGuiTreeNodeFlags_DefaultOpen, "Meshes (%zu)", meshes.size() ) )
		{
			for( const auto& mesh : meshes )
			{
				if( ImGui::TreeNode( &mesh, "Mesh: %s", cmf::ToStdString( mesh.name ).c_str() ) )
				{
					if( !mesh.lods.empty() )
					{
						if( ImGui::TreeNode( &mesh.lods, "LODs (%zu)", mesh.lods.size() ) )
						{
							for( const auto& lod : mesh.lods )
							{
								std::string lodLabel = "LOD " + std::to_string( std::distance( mesh.lods.begin(), &lod ) );
								if( lod.threshold == cmf::MeshLod::MAX_THRESHOLD )
								{
									lodLabel += " (base)";
								}
								else
								{
									lodLabel += " (threshold: " + std::to_string( lod.threshold ) + "px)";
								}
								if( ImGui::TreeNode( &lod, "%s", lodLabel.c_str() ) )
								{
									const uint32_t vertexCount = lod.vb.stride > 0 ? lod.vb.size / lod.vb.stride : 0;
									const uint32_t indexCount = lod.ib.stride > 0 ? lod.ib.size / lod.ib.stride : 0;

									clickableNode( SelectedItem::Type::VertexBuffer, lod.vb, std::string( "Vertices: " + std::to_string( vertexCount ) ).c_str() );
									clickableNode( SelectedItem::Type::IndexBuffer, lod.ib, std::string( "Indices: " + std::to_string( indexCount ) ).c_str() );
									ImGui::TreePop();
								}
							}
							ImGui::TreePop();
						}
					}
					if( !mesh.areas.empty() )
					{
						if( ImGui::TreeNode( &mesh.areas, "Mesh Areas (%zu)", mesh.areas.size() ) )
						{
							for( const auto& area : mesh.areas )
							{
								if( ImGui::TreeNode( &area, "%s", area.name.empty() ? "Unnamed Area" : cmf::ToStdString( area.name ).c_str() ) )
								{
									textNode( "Affected by Bones: %s", area.affectedByBones ? "Yes" : "No" );
									textNode( "Affected by Morphs: %s", area.affectedByMorphTargets ? "Yes" : "No" );
									if( !area.bones.empty() )
									{
										if( ImGui::TreeNode( &area.bones, "Bones" ) )
										{
											for( const auto& boneIndex : area.bones )
											{
												const auto& binding = mesh.boneBindings[boneIndex];
												clickableNode( SelectedItem::Type::BoneBindings, mesh.boneBindings, ToStdString( binding.name ).c_str() );
												textNode(
													"Bounds: [%.4f, %.4f, %.4f] - [%.4f, %.4f, %.4f]",
													binding.bounds.m_min.x,
													binding.bounds.m_min.y,
													binding.bounds.m_min.z,
													binding.bounds.m_max.x,
													binding.bounds.m_max.y,
													binding.bounds.m_max.z );
											}
											ImGui::TreePop();
										}
									}
									ImGui::TreePop();
								}
							}
							ImGui::TreePop();
						}
					}
					if( !mesh.boneBindings.empty() )
					{
						clickableNode( SelectedItem::Type::BoneBindings, mesh.boneBindings, "Bone Bindings" );
					}
					if( !mesh.morphTargets.targets.empty() )
					{
						if( ImGui::TreeNode( &mesh.morphTargets.targets, "Morph Targets (%zu)", mesh.morphTargets.targets.size() ) )
						{
							for( size_t ti = 0; ti < mesh.morphTargets.targets.size(); ++ti )
							{
								const auto& target = mesh.morphTargets.targets[ti];
								if( ImGui::TreeNode( &target, "%s", cmf::ToStdString( target.name ).c_str() ) )
								{
									textNode( "Max Displacement: %.4f", target.maxDisplacement );
									for( int li = 0; li < (int)mesh.lods.size(); ++li )
									{
										const auto& lod = mesh.lods[li];
										if( ti < lod.morphTargets.size() )
										{
											const auto& morphLod = lod.morphTargets[ti];
											const uint32_t vtxCount = morphLod.vb.stride > 0 ? morphLod.vb.size / morphLod.vb.stride : 0;
											clickableNode( SelectedItem::VertexBuffer, morphLod.vb, std::string( "LOD " + std::to_string( li ) + ": " + std::to_string( vtxCount ) + " vertices" ).c_str() );
										}
									}
									ImGui::TreePop();
								}
							}
							ImGui::TreePop();
						}
					}
					textNode( "Bounds Min:  %.4f  %.4f  %.4f", mesh.bounds.m_min.x, mesh.bounds.m_min.y, mesh.bounds.m_min.z );
					textNode( "Bounds Max:  %.4f  %.4f  %.4f", mesh.bounds.m_max.x, mesh.bounds.m_max.y, mesh.bounds.m_max.z );
					textNode( "Bounds Size: %.4f  %.4f  %.4f", mesh.bounds.Size().x, mesh.bounds.Size().y, mesh.bounds.Size().z );

					if( mesh.skeleton != 0xff && (size_t)mesh.skeleton < skeletons.size() )
					{
						const auto& skeleton = skeletons[mesh.skeleton];
						renderSkeleton( skeleton );
					}
					if( !mesh.audioOcclusionMesh.vertices.empty() )
					{
						clickableNode( SelectedItem::Type::AudioOcclusionMesh, mesh.audioOcclusionMesh, "Audio Occlusion Mesh" );
					}
					ImGui::TreePop();
				}
			}
			ImGui::TreePop(); // Meshes
		}

		if( !skeletons.empty() && ImGui::TreeNodeEx( &skeletons, ImGuiTreeNodeFlags_DefaultOpen, "Skeletons (%zu)", skeletons.size() ) )
		{
			for( int si = 0; si < (int)skeletons.size(); ++si )
			{
				const auto& skeleton = skeletons[si];
				renderSkeleton( skeleton );
			}
			ImGui::TreePop();
		}

		if( !cmfContent.m_cmfData->animations.empty() && ImGui::TreeNodeEx( &cmfContent.m_cmfData->animations, ImGuiTreeNodeFlags_DefaultOpen, "Animations (%zu)", cmfContent.m_cmfData->animations.size() ) )
		{
			for( auto& anim : cmfContent.m_cmfData->animations )
			{
				clickableNode( SelectedItem::Animation, anim, ToStdString( anim.name ).c_str() );
			}
			ImGui::TreePop();
		}

		ImGui::TreePop();
	}

	return selectedItem;
}

void UIRenderer::RenderAnimationChannelsSubTab( const cmf::Animation& anim, const cmf::Data& data )
{
	ImGui::Text( "Name: %s", ToStdString( anim.name ).c_str() );
	ImGui::Text( "Duration: %.4f s", anim.duration );
	ImGui::Text( "Channels: %u   Curves: %u", (uint32_t)anim.channels.size(), (uint32_t)anim.curves.size() );

	ImGui::Separator();

	if( anim.channels.empty() )
	{
		ImGui::Text( "No channels" );
		return;
	}

	ImGuiTableFlags tableFlags =
		ImGuiTableFlags_Borders |
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_ScrollX |
		ImGuiTableFlags_ScrollY |
		ImGuiTableFlags_SizingFixedFit;

	if( ImGui::BeginTable( "##channelstable", 3, tableFlags, ImVec2( 0.0f, ImGui::GetContentRegionAvail().y ) ) )
	{
		ImGui::TableSetupScrollFreeze( 0, 1 );
		ImGui::TableSetupColumn( "Target", ImGuiTableColumnFlags_WidthStretch );
		ImGui::TableSetupColumn( "Type", ImGuiTableColumnFlags_WidthFixed );
		ImGui::TableSetupColumn( "Curve", ImGuiTableColumnFlags_WidthFixed );

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
				if( std::find( m_selectedItem.selectedIndices.begin(), m_selectedItem.selectedIndices.end(), uint32_t( i ) ) != m_selectedItem.selectedIndices.end() )
				{
					ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32( ImGuiCol_Header ) );
				}

				ImGui::TableSetColumnIndex( 0 );
				ImGui::PushID( i );
				bool foundTarget = false;
				switch( channel.targetType )
				{
				case cmf::AnimationChannelTargetType::BonePosition:
				case cmf::AnimationChannelTargetType::BoneRotation:
				case cmf::AnimationChannelTargetType::BoneScale:
					for( auto& skeleton : data.skeletons )
					{
						auto bone = std::find( skeleton.bones.begin(), skeleton.bones.end(), channel.target );
						if( bone != skeleton.bones.end() )
						{
							if( ImGui::TextLink( cmf::ToStdString( *bone ).c_str() ) )
							{
								m_selectedItem = SelectedItem{ SelectedItem::Type::SkeletonBones, &skeleton, { (uint32_t)std::distance( skeleton.bones.begin(), bone ) }, true };
							}
							foundTarget = true;
							break;
						}
					}
					break;
				case cmf::AnimationChannelTargetType::MorphTarget:
					for( auto& mesh : data.meshes )
					{
						for( auto& target : mesh.morphTargets.targets )
						{
							if( target.name == channel.target )
							{
								if( ImGui::TextLink( cmf::ToStdString( target.name ).c_str() ) )
								{
									m_selectedItem = SelectedItem{ SelectedItem::Type::VertexBuffer, &mesh.lods[0].morphTargets[std::distance( mesh.morphTargets.targets.begin(), &target )].vb };
								}
								foundTarget = true;
								break;
							}
						}
						if( foundTarget )
						{
							break;
						}
					}
					break;
				default:
					break;
				}
				if( !foundTarget )
				{
					ImGui::TextUnformatted( cmf::ToStdString( channel.target ).c_str() );
				}
				ImGui::PopID();

				ImGui::TableSetColumnIndex( 1 );
				ImGui::TextUnformatted( typeName );

				ImGui::TableSetColumnIndex( 2 );
				ImGui::PushID( i );
				if( ImGui::TextLink( std::to_string( channel.curveIndex ).c_str() ) )
				{
					m_selectedItem = SelectedItem{ SelectedItem::Type::AnimationCurve, &anim.curves[channel.curveIndex] };
				}
				ImGui::PopID();
			}
		}
		clipper.End();
		ImGui::EndTable();
	}
}

void UIRenderer::RenderAnimationCurvesSubTab( const cmf::AnimationCurve& curve, const cmf::Animation& anim )
{
	const uint32_t curveIndex = uint32_t( std::distance( anim.curves.begin(), &curve ) );
	ImGui::Text( "Index: %u", curveIndex );
	ImGui::Text( "Knots: %u", curve.knotCount );
	ImGui::Text( "Knot Type: %s", GetElementTypeName( curve.knotType ) );
	ImGui::Text( "Value Type: %s%s", 
		GetElementTypeName( curve.valueType ), 
		curve.valueDimension > 1 ? ( " x" + std::to_string( curve.valueDimension ) ).c_str() : "" );
	ImGui::Text( "Interpolation: %s", curve.interpolation == cmf::Interpolation::Linear ? "Linear" : "Step" );

	const uint32_t targetCount = std::accumulate( anim.channels.begin(), anim.channels.end(), 0u, [&]( uint32_t sum, const cmf::AnimationChannel& channel ) {
		return sum + ( channel.curveIndex == curveIndex ? 1 : 0 );
	} );
	ImGui::Text( "Targets: " );
	ImGui::SameLine();
	if( targetCount )
	{
		if( ImGui::TextLink( std::to_string( targetCount ).c_str() ) )
		{
			std::vector<uint32_t> linkedChannels;
			for( uint32_t ci = 0; ci < (uint32_t)anim.channels.size(); ++ci )
			{
				if( anim.channels[ci].curveIndex == curveIndex )
				{
					linkedChannels.push_back( ci );
				}
			}
			m_selectedItem = SelectedItem{ SelectedItem::Type::Animation, &anim, linkedChannels };
		}
	}
	else
	{
		ImGui::Text( "%u", targetCount );
	}

	const ImGuiTableFlags tableFlags =
		ImGuiTableFlags_Borders |
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_ScrollX |
		ImGuiTableFlags_ScrollY |
		ImGuiTableFlags_SizingFixedFit;

	if( ImGui::BeginTable( "##curvestable", 3, tableFlags, ImVec2( 0.0f, ImGui::GetContentRegionAvail().y ) ) )
	{
		ImGui::TableSetupScrollFreeze( 1, 1 );
		ImGui::TableSetupColumn( "Knot", ImGuiTableColumnFlags_WidthFixed, 48.0f );
		ImGui::TableSetupColumn( "Time", ImGuiTableColumnFlags_WidthFixed, 80.0f );
		ImGui::TableSetupColumn( "Value", ImGuiTableColumnFlags_WidthFixed );
		ImGui::TableHeadersRow();

		cmf::VertexElement element = {};
		element.type = curve.knotType;
		element.elementCount = 1;
		const auto stride = cmf::GetVertexElementSize( element );
		cmf::ConstBufferElementStream<float>  knots{ element, curve.knots.data(), uint32_t( curve.knots.size() / stride ), stride };

		element.type = curve.valueType;
		const auto valueStride = cmf::GetVertexElementSize( element );
		cmf::ConstBufferElementStream<float> values{ element, curve.values.data(), uint32_t( curve.values.size() / valueStride ), valueStride };

		ImGuiListClipper clipper;
		clipper.Begin( int( curve.knotCount ) );
		while( clipper.Step() )
		{
			for( int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i )
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex( 0 );
				ImGui::Text( "%d", i );
				ImGui::TableSetColumnIndex( 1 );
				ImGui::Text( "%.2f", knots[i] );
				ImGui::TableSetColumnIndex( 2 );
				switch( curve.valueDimension )
				{
				case 1:
					ImGui::Text( "%.4f", values[i * curve.valueDimension] );
					break;
				case 2:
					ImGui::Text( "%.4f  %.4f", values[i * curve.valueDimension], values[i * curve.valueDimension + 1] );
					break;
				case 3:
					ImGui::Text( "%.4f  %.4f  %.4f", values[i * curve.valueDimension], values[i * curve.valueDimension + 1], values[i * curve.valueDimension + 2] );
					break;
				case 4:
					ImGui::Text( "%.4f  %.4f  %.4f  %.4f", values[i * curve.valueDimension], values[i * curve.valueDimension + 1], values[i * curve.valueDimension + 2], values[i * curve.valueDimension + 3] );
					break;
				default:
					break;
				}
			}
		}
		clipper.End();
		ImGui::EndTable();
	}
}

void UIRenderer::RenderAudioOccluderTab( const cmf::AudioOcclusionMesh& audioOcclusionMesh )
{
	if( audioOcclusionMesh.vertices.empty() && audioOcclusionMesh.indices.empty() )
	{
		ImGui::Text( "No audio occlusion mesh" );
		return;
	}

	const uint32_t vertexCount = uint32_t( audioOcclusionMesh.vertices.size() );
	const uint32_t triCount = uint32_t( audioOcclusionMesh.indices.size() / 3 );

	ImGui::Text( "Vertices: %u  Triangles: %u", vertexCount, triCount );
	const auto& b = audioOcclusionMesh.bounds;
	auto sz = b.Size();
	ImGui::Text( "Bounds Min:  %.4f  %.4f  %.4f", b.m_min.x, b.m_min.y, b.m_min.z );
	ImGui::Text( "Bounds Max:  %.4f  %.4f  %.4f", b.m_max.x, b.m_max.y, b.m_max.z );
	ImGui::Text( "Bounds Size: %.4f  %.4f  %.4f", sz.x, sz.y, sz.z );

	ImGui::Spacing();

	if( ImGui::CollapsingHeader( "Vertices", ImGuiTreeNodeFlags_DefaultOpen ) )
	{
		const ImGuiTableFlags tableFlags =
			ImGuiTableFlags_Borders |
			ImGuiTableFlags_RowBg |
			ImGuiTableFlags_ScrollX |
			ImGuiTableFlags_ScrollY |
			ImGuiTableFlags_SizingFixedFit;

		const float tableHeight = std::min( (float)vertexCount * ImGui::GetTextLineHeightWithSpacing() + ImGui::GetTextLineHeightWithSpacing(), 200.0f );
		if( ImGui::BeginTable( "##aomverts", 4, tableFlags, ImVec2( 0.0f, tableHeight ) ) )
		{
			ImGui::TableSetupScrollFreeze( 1, 1 );
			ImGui::TableSetupColumn( "Index", ImGuiTableColumnFlags_WidthFixed, 60.0f );
			ImGui::TableSetupColumn( "X", ImGuiTableColumnFlags_WidthFixed, 80.0f );
			ImGui::TableSetupColumn( "Y", ImGuiTableColumnFlags_WidthFixed, 80.0f );
			ImGui::TableSetupColumn( "Z", ImGuiTableColumnFlags_WidthStretch );
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
					ImGui::TableSetColumnIndex( 1 );
					ImGui::Text( "%.4f", v.x );
					ImGui::TableSetColumnIndex( 2 );
					ImGui::Text( "%.4f", v.y );
					ImGui::TableSetColumnIndex( 3 );
					ImGui::Text( "%.4f", v.z );
				}
			}
			clipper.End();
			ImGui::EndTable();
		}
	}

	ImGui::Spacing();

	if( triCount > 0 && ImGui::CollapsingHeader( "Triangles", ImGuiTreeNodeFlags_DefaultOpen ) )
	{
		const ImGuiTableFlags tableFlags =
			ImGuiTableFlags_Borders |
			ImGuiTableFlags_RowBg |
			ImGuiTableFlags_ScrollX |
			ImGuiTableFlags_ScrollY |
			ImGuiTableFlags_SizingFixedFit;

		const float tableHeight = std::min( (float)triCount * ImGui::GetTextLineHeightWithSpacing() + ImGui::GetTextLineHeightWithSpacing(), 200.0f );
		if( ImGui::BeginTable( "##aomtris", 4, tableFlags, ImVec2( 0.0f, tableHeight ) ) )
		{
			ImGui::TableSetupScrollFreeze( 1, 1 );
			ImGui::TableSetupColumn( "Triangle", ImGuiTableColumnFlags_WidthFixed, 60.0f );
			ImGui::TableSetupColumn( "V0", ImGuiTableColumnFlags_WidthFixed, 80.0f );
			ImGui::TableSetupColumn( "V1", ImGuiTableColumnFlags_WidthFixed, 80.0f );
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

// NOLINTEND(cppcoreguidelines-pro-type-vararg)
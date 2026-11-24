// Copyright © 2026 CCP ehf.

#include "imguiLayer.h"

#include <array>

#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include "vulkan/rendertarget.h"
#include "vulkan/vulkanerrors.h"

namespace
{
// descriptors for textures registered with AddTexture (e.g. render target previews)
constexpr uint32_t USER_TEXTURE_DESCRIPTORS = 16;

void CheckVkResult( VkResult err )
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
}

ImGuiLayer::ImGuiLayer( std::shared_ptr<const Renderer> renderer ) :
	m_renderer( std::move( renderer ) ),
	m_graphicsCommandBuffer( m_renderer.get() )
{
}

ImGuiLayer::~ImGuiLayer()
{
	if( m_initialized )
	{
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
	}
	if( ImGui::GetCurrentContext() != nullptr )
	{
		ImGui::DestroyContext();
	}
	if( m_descriptorPool != VK_NULL_HANDLE )
	{
		vkDestroyDescriptorPool( m_renderer->GetDevice()->GetLogicalDevice(), m_descriptorPool, m_renderer->GetAllocator() );
	}
}

void ImGuiLayer::CreateContext()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
}

VkResult ImGuiLayer::InitializeBackend( GLFWwindow* window, uint32_t width, uint32_t height )
{
	const Device* device = m_renderer->GetDevice();
	const Swapchain* swapchain = m_renderer->GetSwapchain();
	VkDevice logicalDevice = device->GetLogicalDevice();
	const VkAllocationCallbacks* allocator = m_renderer->GetAllocator();

	const auto graphicsFamily = device->GetQueueFamilyIndices().graphicsFamily;
	if( !graphicsFamily.has_value() )
	{
		Log::Error( "No graphics queue family for ImGui" );
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	std::array<VkDescriptorPoolSize, 1> poolSizes = { {
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE + USER_TEXTURE_DESCRIPTORS },
	} };
	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	poolInfo.maxSets = 0;
	for( const VkDescriptorPoolSize& poolSize : poolSizes )
	{
		poolInfo.maxSets += poolSize.descriptorCount;
	}
	poolInfo.poolSizeCount = static_cast<uint32_t>( poolSizes.size() );
	poolInfo.pPoolSizes = poolSizes.data();

	CR_RETURN( vkCreateDescriptorPool( logicalDevice, &poolInfo, allocator, &m_descriptorPool ) );

	ImGui_ImplGlfw_InitForVulkan( window, true );
	ImGui_ImplVulkan_InitInfo initInfo = {}; // NOLINT(bugprone-invalid-enum-default-initialization) C struct, all members are set below or valid when zero
	initInfo.Instance = m_renderer->GetVulkanInstance();
	initInfo.PhysicalDevice = device->GetPhysicalDevice();
	initInfo.Device = logicalDevice;
	initInfo.QueueFamily = *graphicsFamily;
	initInfo.Queue = device->GetGraphicsQueue();
	initInfo.DescriptorPool = m_descriptorPool;
	initInfo.MinImageCount = swapchain->GetMinImageCount();
	initInfo.ImageCount = swapchain->GetImageCount();
	initInfo.Allocator = allocator;
	initInfo.RenderPass = VK_NULL_HANDLE;
	initInfo.Subpass = 0;
	initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	initInfo.CheckVkResultFn = CheckVkResult;
	initInfo.UseDynamicRendering = true;

	// dynamic rendering parameters for imgui to use
	auto swapchainFormat = swapchain->GetFormat();
	initInfo.PipelineRenderingCreateInfo = {};
	initInfo.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	initInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
	initInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &swapchainFormat;
	initInfo.PipelineRenderingCreateInfo.depthAttachmentFormat = m_renderer->GetDepthTexture()->GetFormat();
	initInfo.PipelineRenderingCreateInfo.stencilAttachmentFormat = m_renderer->GetDepthTexture()->GetFormat();

	ImGui_ImplVulkan_Init( &initInfo );
	m_initialized = true;

	SetRenderSize( width, height );
	return VK_SUCCESS;
}

void ImGuiLayer::SetRenderSize( uint32_t width, uint32_t height )
{
	m_graphicsCommandBuffer.SetRenderSize( width, height );
}

void ImGuiLayer::UpdateFontsTexture()
{
	ImGui_ImplVulkan_CreateFontsTexture();
}

void ImGuiLayer::BeginFrame()
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void ImGuiLayer::Render()
{
	m_graphicsCommandBuffer.Begin( m_renderer.get() );
	ImGui::Render();

	ImGui_ImplVulkan_RenderDrawData( ImGui::GetDrawData(), m_renderer->GetCurrentGraphicVkCommandBuffer() );
	m_graphicsCommandBuffer.End();
}

ImTextureID ImGuiLayer::AddTexture( const RenderTarget& target )
{
	if( !target.IsValid() )
	{
		return ImTextureID{};
	}
	VkDescriptorSet set = ImGui_ImplVulkan_AddTexture( target.GetSampler(), target.GetColorTexture()->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	return reinterpret_cast<ImTextureID>( set ); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast) ImGui stores Vulkan descriptor sets as texture ids
}

void ImGuiLayer::RemoveTexture( ImTextureID texture )
{
	if( texture != ImTextureID{} )
	{
		ImGui_ImplVulkan_RemoveTexture( reinterpret_cast<VkDescriptorSet>( texture ) ); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,performance-no-int-to-ptr) ImGui stores Vulkan descriptor sets as texture ids
	}
}

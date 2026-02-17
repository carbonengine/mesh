#pragma once
#include "../renderer.h"


// Effect class that dictates how shaders are set up
class Effect
{
public:
	Effect( std::shared_ptr<const Renderer> renderer );
	~Effect();

	struct Config
	{
		VkPrimitiveTopology topology{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };
		VkPolygonMode polygonMode{ VK_POLYGON_MODE_FILL };
		float lineWidth{ 1.0f };
		VkCompareOp depthCompareOp{ VK_COMPARE_OP_LESS };
		VkCullModeFlags cullMode{ VK_CULL_MODE_BACK_BIT };
		bool blend{ false };
		uint32_t vertexStride{ 0 };
		std::vector<VkVertexInputAttributeDescription> vertexDescriptions{};
	};

	VkResult Initialize();

	void Bind( VkCommandBuffer commandBuffer, uint32_t currentFrameIndex );

	void SetShaderName( std::string name );
	void SetConfig( Effect::Config config );

	template <typename T>
	void RegisterUniformData( VkShaderStageFlagBits stage, uint32_t layoutBindingIndex )
	{
		RegisterUniformData( stage, layoutBindingIndex, nullptr, sizeof( T ) );
	};

	template <typename T>
	void RegisterUniformData( VkShaderStageFlagBits stage, uint32_t layoutBindingIndex, const T& initialData )
	{
		RegisterUniformData( stage, layoutBindingIndex, reinterpret_cast<const uint8_t*>( &initialData ), sizeof( T ) );
	};

	template <typename T>
	void SetUniformData( uint32_t layoutBindingIndex, const T& data )
	{
		SetUniformData( layoutBindingIndex, reinterpret_cast<const uint8_t*>( &data ), sizeof( T ) );
	};

	void RegisterUniformData( VkShaderStageFlagBits stage, uint32_t layoutBindingIndex, const uint8_t* data, size_t dataSize );

	bool IsInitialized();

private:
	void SetUniformData( uint32_t layoutBindingIndex, const uint8_t* data, size_t dataSize );
	VkResult InitializeDescriptors();
	VkResult InitializeBuffers();
	VkResult InitializePipeline();
	VkResult RecreatePipeline();

	struct UniformBufferMemory
	{
		VkDeviceMemory memory{ VK_NULL_HANDLE };
		VkBuffer buffer{ VK_NULL_HANDLE };
		uint8_t* mapped{ nullptr };
		VkDescriptorBufferInfo descriptor{};
	};

	struct UniformBuffer
	{
		VkShaderStageFlagBits stage{ VkShaderStageFlagBits::VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM };
		uint32_t layoutBindingIndex{ 0u };
		size_t expectedSize{ 0 };
		std::array<UniformBufferMemory, RenderingConsts::MAX_FRAMES_IN_FLIGHT> buffers{};
		std::vector<uint8_t> initialData{};
	};

	std::array<VkDescriptorSet, RenderingConsts::MAX_FRAMES_IN_FLIGHT> m_descriptorSets{};
	std::vector<UniformBuffer> m_uniformBuffers;


	std::shared_ptr<const Renderer> m_renderer{ nullptr };
	Config m_config{};
	std::string m_shaderName{ "" };
	VkPipeline m_pipeline{ VK_NULL_HANDLE };

	VkDescriptorSetLayout m_descriptorSetLayout{ VK_NULL_HANDLE };
	VkPipelineLayout m_pipelineLayout{ VK_NULL_HANDLE };
	VkDescriptorPool m_descriptorPool{ VK_NULL_HANDLE };
	bool m_initialized{ false };
};

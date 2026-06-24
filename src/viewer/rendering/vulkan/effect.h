// Copyright © 2026 CCP ehf.

#pragma once
#include "../renderer.h"
#include "buffer.h"

// Effect class that dictates how shaders are set up
class Effect
{
public:
	Effect();
	Effect( std::shared_ptr<const Renderer> renderer );
	~Effect();

	VkResult Initialize();

	virtual void Bind( VkCommandBuffer commandBuffer, uint32_t currentFrameIndex ) = 0;

	void SetShaderName( std::string name );

	void RegisterStorageBuffer( VkShaderStageFlagBits stage, uint32_t layoutBindingIndex, Buffer* buffer );

	template <typename T>
	void RegisterStorageBuffer( VkShaderStageFlagBits stage, uint32_t layoutBindingIndex, const T* data, size_t elementCount )
	{
		RegisterStorageBuffer( stage, layoutBindingIndex, reinterpret_cast<const uint8_t*>( data ), elementCount * sizeof( T ), sizeof( T ) );
	};

	template <typename T>
	void RegisterStorageBuffer( VkShaderStageFlagBits stage, uint32_t layoutBindingIndex, size_t elementCount )
	{
		RegisterStorageBuffer( stage, layoutBindingIndex, nullptr, elementCount * sizeof( T ), sizeof( T ) );
	};

	template <typename T>
	void SetStorageBufferData( uint32_t layoutBindingIndex, const T* data, size_t elementCount )
	{
		SetStorageBuffer( layoutBindingIndex, reinterpret_cast<const uint8_t*>( data ), elementCount * sizeof( T ) );
	};

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

	bool IsInitialized();

protected:
	void RegisterStorageBuffer( VkShaderStageFlagBits stage, uint32_t layoutBindingIndex, const uint8_t* data, size_t dataSize, size_t elementSize );
	void RegisterUniformData( VkShaderStageFlagBits stage, uint32_t layoutBindingIndex, const uint8_t* data, size_t dataSize );

	void SetStorageBuffer( uint32_t layoutBindingIndex, const uint8_t* data, size_t dataSize );
	void SetUniformData( uint32_t layoutBindingIndex, const uint8_t* data, size_t dataSize );

	virtual VkResult CreatePipeline() = 0;

	struct UniformBufferMemory
	{
		VkDeviceMemory memory{ VK_NULL_HANDLE };
		VkBuffer buffer{ VK_NULL_HANDLE };
		uint8_t* mapped{ nullptr };
		VkDescriptorBufferInfo descriptor{};
	};

	struct StorageBuffer
	{
		Buffer* buffer{ nullptr };
		uint32_t layoutBindingIndex{ 0u };
		VkShaderStageFlagBits stage{ VkShaderStageFlagBits::VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM };
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
	std::vector<UniformBuffer> m_uniformBuffers{};
	std::vector<StorageBuffer> m_storageBuffers{};

	std::shared_ptr<const Renderer> m_renderer{ nullptr };
	std::string m_shaderName{ "" };
	VkPipeline m_pipeline{ VK_NULL_HANDLE };

	VkDescriptorSetLayout m_descriptorSetLayout{ VK_NULL_HANDLE };
	VkPipelineLayout m_pipelineLayout{ VK_NULL_HANDLE };
	VkDescriptorPool m_descriptorPool{ VK_NULL_HANDLE };
	bool m_initialized{ false };

	VkResult InitializeDescriptors();
	VkResult InitializeBuffers();
	VkResult InitializePipeline();
	VkResult RecreatePipeline();
};

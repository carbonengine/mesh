// Copyright © 2026 CCP ehf.

#include "effect.h"
#include "vulkanerrors.h"

Effect::Effect()
{
}

Effect::Effect( std::shared_ptr<const Renderer> renderer ) :
	m_renderer( renderer )
{
}

Effect::~Effect()
{
	if( m_initialized )
	{
		auto device = m_renderer->GetDevice()->GetLogicalDevice();
		auto allocator = m_renderer->GetAllocator();
		if( m_pipeline != VK_NULL_HANDLE )
		{
			vkDestroyPipeline( device, m_pipeline, allocator );
		}
		if( m_pipelineLayout != VK_NULL_HANDLE )
		{
			vkDestroyPipelineLayout( device, m_pipelineLayout, allocator );
		}

		for( auto& uniformBuffer : m_uniformBuffers )
		{
			for( auto& uniformBufferMemory : uniformBuffer.buffers )
			{
				if( uniformBufferMemory.buffer != VK_NULL_HANDLE )
				{
					vkDestroyBuffer( device, uniformBufferMemory.buffer, allocator );
					uniformBufferMemory.buffer = VK_NULL_HANDLE;
				}
				if( uniformBufferMemory.memory != VK_NULL_HANDLE )
				{
					vkUnmapMemory( device, uniformBufferMemory.memory );
					vkFreeMemory( device, uniformBufferMemory.memory, allocator );
					uniformBufferMemory.memory = VK_NULL_HANDLE;
				}
			}
		}

		if( m_descriptorSetLayout != VK_NULL_HANDLE )
		{
			vkDestroyDescriptorSetLayout( device, m_descriptorSetLayout, allocator );
		}

		if( m_descriptorPool != VK_NULL_HANDLE )
		{
			vkDestroyDescriptorPool( device, m_descriptorPool, allocator );
		}
	}
}

VkResult Effect::Initialize()
{
	m_initialized = false;
	RETURN_ERROR( InitializeBuffers() );
	RETURN_ERROR( InitializeDescriptors() );
	RETURN_ERROR( InitializePipeline() );
	m_initialized = true;

	return VK_SUCCESS;
}

bool Effect::IsInitialized()
{
	return m_initialized;
}

VkResult Effect::InitializeDescriptors()
{
	auto device = m_renderer->GetDevice()->GetLogicalDevice();
	auto allocator = m_renderer->GetAllocator();


	std::vector<VkDescriptorPoolSize> poolSizes = {};

	if( !m_uniformBuffers.empty() )
	{
		poolSizes.push_back( { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, (uint32_t)( RenderingConsts::MAX_FRAMES_IN_FLIGHT * m_uniformBuffers.size() ) } );
	}
	if( !m_storageBuffers.empty() )
	{
		poolSizes.push_back( { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, (uint32_t)( RenderingConsts::MAX_FRAMES_IN_FLIGHT * m_storageBuffers.size() ) } );
	}

	VkDescriptorPoolCreateInfo descriptorPoolInfo{};
	descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolInfo.pPoolSizes = poolSizes.data();
	descriptorPoolInfo.poolSizeCount = (uint32_t)poolSizes.size();
	descriptorPoolInfo.maxSets = (uint32_t)( RenderingConsts::MAX_FRAMES_IN_FLIGHT * poolSizes.size() );
	RETURN_LOG_ERROR( vkCreateDescriptorPool( device, &descriptorPoolInfo, allocator, &m_descriptorPool ), "Could not initialize descriptor pool" );

	// layout
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};

	for( auto& uniformBuffer : m_uniformBuffers )
	{
		VkDescriptorSetLayoutBinding setLayoutBinding{};
		setLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		setLayoutBinding.stageFlags = uniformBuffer.stage;
		setLayoutBinding.binding = uniformBuffer.layoutBindingIndex;
		setLayoutBinding.descriptorCount = 1;
		setLayoutBindings.push_back( setLayoutBinding );
	}

	for( auto& storageBuffer : m_storageBuffers )
	{
		VkDescriptorSetLayoutBinding setLayoutBinding{};
		setLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		setLayoutBinding.stageFlags = storageBuffer.stage;
		setLayoutBinding.binding = storageBuffer.layoutBindingIndex;
		setLayoutBinding.descriptorCount = 1;
		setLayoutBindings.push_back( setLayoutBinding );
	}

	VkDescriptorSetLayoutCreateInfo descriptorLayout{};
	descriptorLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorLayout.pBindings = setLayoutBindings.data();
	descriptorLayout.bindingCount = (uint32_t)setLayoutBindings.size();
	RETURN_LOG_ERROR( vkCreateDescriptorSetLayout( device, &descriptorLayout, nullptr, &m_descriptorSetLayout ), "Failed to create descriptor layout for effect" );

	// allocate buffer info
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = m_descriptorPool;
	allocInfo.pSetLayouts = &m_descriptorSetLayout;
	allocInfo.descriptorSetCount = 1;

	for( auto i = 0; i < RenderingConsts::MAX_FRAMES_IN_FLIGHT; i++ )
	{
		RETURN_LOG_ERROR( vkAllocateDescriptorSets( device, &allocInfo, &m_descriptorSets[i] ), "Failed to allocate descriptor sets" );
		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {};
		for( auto& uniformBuffer : m_uniformBuffers )
		{
			VkWriteDescriptorSet writeDescriptorSet{};
			writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSet.dstSet = m_descriptorSets[i];
			writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			writeDescriptorSet.dstBinding = uniformBuffer.layoutBindingIndex;
			uniformBuffer.buffers[i].descriptor = {};
			uniformBuffer.buffers[i].descriptor.buffer = uniformBuffer.buffers[i].buffer;
			uniformBuffer.buffers[i].descriptor.range = uniformBuffer.expectedSize;

			writeDescriptorSet.pBufferInfo = &uniformBuffer.buffers[i].descriptor;
			writeDescriptorSet.descriptorCount = 1;
			writeDescriptorSets.push_back( writeDescriptorSet );
		}
		for( auto& storageBuffer : m_storageBuffers )
		{
			auto& bufferInfo = storageBuffer.buffer->GetDescriptorBufferInfo();
			VkWriteDescriptorSet writeDescriptorSet{};
			writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSet.dstSet = m_descriptorSets[i];
			writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			writeDescriptorSet.dstBinding = storageBuffer.layoutBindingIndex;
			writeDescriptorSet.pBufferInfo = &bufferInfo;
			writeDescriptorSet.descriptorCount = 1;
			writeDescriptorSets.push_back( writeDescriptorSet );
		}
		vkUpdateDescriptorSets( device, static_cast<uint32_t>( writeDescriptorSets.size() ), writeDescriptorSets.data(), 0, nullptr );
	}
	return VK_SUCCESS;
}

VkResult Effect::InitializeBuffers()
{
	auto device = m_renderer->GetDevice()->GetLogicalDevice();
	auto allocator = m_renderer->GetAllocator();


	for( auto& uniformBuffer : m_uniformBuffers )
	{
		VkBufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferCreateInfo.size = uniformBuffer.expectedSize;
		bufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

		// Vertex shader uniform buffer block
		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.pNext = nullptr;
		allocInfo.allocationSize = 0;
		allocInfo.memoryTypeIndex = 0;

		// Create the buffers
		for( uint32_t i = 0; i < RenderingConsts::MAX_FRAMES_IN_FLIGHT; i++ )
		{
			VkMemoryRequirements memReqs{};
			auto& uniformBufferMemory = uniformBuffer.buffers[i];
			RETURN_LOG_ERROR( vkCreateBuffer( device, &bufferCreateInfo, allocator, &uniformBufferMemory.buffer ), "Failed to create perframe buffer" );
			vkGetBufferMemoryRequirements( device, uniformBufferMemory.buffer, &memReqs );
			allocInfo.allocationSize = memReqs.size;
			allocInfo.memoryTypeIndex = m_renderer->GetDevice()->GetMemoryTypeIndex( memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
			RETURN_LOG_ERROR( vkAllocateMemory( device, &allocInfo, allocator, &uniformBufferMemory.memory ), "Failed to allocate memory for perframe buffer" );
			RETURN_LOG_ERROR( vkBindBufferMemory( device, uniformBufferMemory.buffer, uniformBufferMemory.memory, 0 ), "Failed to bind perframe buffer memory" );
			RETURN_LOG_ERROR( vkMapMemory( device, uniformBufferMemory.memory, 0, uniformBuffer.expectedSize, 0, (void**)&uniformBufferMemory.mapped ), "Failed to map perframe buffer memory" );
			if( !uniformBuffer.initialData.empty() )
			{
				memcpy( uniformBufferMemory.mapped, uniformBuffer.initialData.data(), uniformBuffer.expectedSize );
			}
		}
	}
	return VK_SUCCESS;
}

VkResult Effect::InitializePipeline()
{
	auto device = m_renderer->GetDevice()->GetLogicalDevice();
	auto allocator = m_renderer->GetAllocator();

	std::vector<VkDescriptorSetLayout> setLayouts = { m_descriptorSetLayout };
	VkPipelineLayoutCreateInfo pipelineLayoutCI{};
	pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCI.setLayoutCount = static_cast<uint32_t>( setLayouts.size() );
	pipelineLayoutCI.pSetLayouts = setLayouts.data();
	pipelineLayoutCI.pushConstantRangeCount = 0;
	pipelineLayoutCI.pPushConstantRanges = nullptr;
	RETURN_LOG_ERROR( vkCreatePipelineLayout( device, &pipelineLayoutCI, allocator, &m_pipelineLayout ), "Failed to create pipeline layout" );

	RETURN_LOG_ERROR( CreatePipeline(), "Failed to create pipeline" );
	return VK_SUCCESS;
}

VkResult Effect::RecreatePipeline()
{
	auto device = m_renderer->GetDevice()->GetLogicalDevice();
	auto allocator = m_renderer->GetAllocator();

	vkDestroyPipeline( device, m_pipeline, allocator );
	vkDestroyPipelineLayout( device, m_pipelineLayout, allocator );
	RETURN_ERROR( InitializePipeline() );
	return VK_SUCCESS;
}

void Effect::SetShaderName( std::string name )
{
	m_shaderName = name;

	if( m_initialized )
	{
		m_initialized = RecreatePipeline() == VK_SUCCESS;
	}
}

void Effect::RegisterStorageBuffer( VkShaderStageFlagBits stage, uint32_t layoutBindingIndex, Buffer* buffer )
{
	assert( !m_initialized );
	m_storageBuffers.push_back( { buffer, layoutBindingIndex, stage } );
}

void Effect::RegisterStorageBuffer( VkShaderStageFlagBits stage, uint32_t layoutBindingIndex, const uint8_t* initialData, size_t dataSize, size_t elementSize )
{
	assert( !m_initialized );
	auto foundElement = std::find_if( m_storageBuffers.begin(), m_storageBuffers.end(), [&]( const auto& buff ) {
		return buff.layoutBindingIndex == layoutBindingIndex;
	} );

	assert( foundElement == m_storageBuffers.end() );

	Buffer* buffer = BufferBuilder::Build( m_renderer.get(), initialData, dataSize, BufferType::Storage, elementSize );
	RegisterStorageBuffer( stage, layoutBindingIndex, buffer );
}

void Effect::RegisterUniformData( VkShaderStageFlagBits stages, uint32_t layoutBindingIndex, const uint8_t* initialData, size_t dataSize )
{
	assert( !m_initialized );
	auto foundElement = std::find_if( m_uniformBuffers.begin(), m_uniformBuffers.end(), [&]( auto buff ) {
		return buff.layoutBindingIndex == layoutBindingIndex;
	} );

	assert( foundElement == m_uniformBuffers.end() );

	UniformBuffer buffer = { stages, layoutBindingIndex, dataSize, {}, {} };

	if( initialData != nullptr )
	{
		buffer.initialData.resize( dataSize );
		memcpy( buffer.initialData.data(), initialData, dataSize );
	}

	m_uniformBuffers.push_back( buffer );
}

void Effect::SetStorageBuffer( uint32_t layoutBindingIndex, const uint8_t* data, size_t totalSize )
{
	assert( data != nullptr );
	auto foundElement = std::find_if( m_storageBuffers.begin(), m_storageBuffers.end(), [&]( const auto& buff ) {
		return buff.layoutBindingIndex == layoutBindingIndex;
	} );
	assert( foundElement != m_storageBuffers.end() );
	ON_ERROR_LOG_AND_RETURN( foundElement->buffer->SetData( m_renderer.get(), data, totalSize ), "Failed to set storage buffer data" );
}

void Effect::SetUniformData( uint32_t layoutBindingIndex, const uint8_t* data, size_t dataSize )
{
	assert( data != nullptr );
	auto foundElement = std::find_if( m_uniformBuffers.begin(), m_uniformBuffers.end(), [&]( auto buff ) {
		return buff.layoutBindingIndex == layoutBindingIndex;
	} );
	assert( foundElement != m_uniformBuffers.end() );

	assert( dataSize == foundElement->expectedSize );

	auto mapped = foundElement->buffers[m_renderer->GetCurrentFrame()].mapped;
	memcpy( mapped, data, dataSize );
}

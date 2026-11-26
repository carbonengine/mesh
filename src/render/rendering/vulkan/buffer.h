// Copyright © 2025 CCP ehf.

#pragma once

#include "device.h"
#include "rendering/renderer.h"


enum class BufferType
{
	Vertex,
	Index,
	Storage,
	Undefined
};

// Simple buffer object.
// Holds on to the memory and buffer
class Buffer
{
public:
	Buffer();
	~Buffer();

	void Release( const Renderer* renderer );
	VkResult Initialize( const Renderer* renderer, BufferType type, const uint8_t* data, size_t size, size_t stride );

	VkBuffer GetGpuBuffer() const;
	VkDescriptorBufferInfo& GetDescriptorBufferInfo();

	bool IsValid() const;

	void CopyFromStaging( VkCommandBuffer commandBuffer );
	void ReleaseStaging( const Renderer* renderer );
	VkResult SetData( const Renderer* renderer, const uint8_t* data, size_t size );
	size_t size() const;
	size_t stride() const;


private:
	VkResult CreateBuffer( const Renderer* renderer, BufferType type, const uint8_t* data );
	VkResult CreateStorageBuffer( const Renderer* renderer, const uint8_t* data );
	VkDeviceMemory m_memory;
	VkBuffer m_buffer;
	size_t m_size;
	size_t m_stride;

	VkDeviceMemory m_stagingMemory;
	VkBuffer m_stagingBuffer;
	BufferType m_type{ BufferType::Undefined };
	VkDescriptorBufferInfo m_descriptorInfo{ VK_NULL_HANDLE, 0, VK_WHOLE_SIZE };

	bool m_isValid;
};


namespace BufferBuilder
{
Buffer* Build( const Renderer* renderer, const uint8_t* data, size_t size, BufferType type, size_t stride );
}
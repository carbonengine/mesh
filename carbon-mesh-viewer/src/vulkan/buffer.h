#pragma once
#include "device.h"
#include <cmf/cmf.h>
#include <vulkan/vulkan.h>

enum BufferType
{
	BufferTypeVertex,
	BufferTypeIndex
};

class Buffer
{
public:
	Buffer();
	~Buffer();

	void Release( Device* device, VkAllocationCallbacks* allocator );
	VkResult Initialize( Device* device, BufferType type, const uint8_t* data, uint32_t size, uint32_t offset, uint32_t stride, uint32_t index );

	VkBuffer GetGpuBuffer() const
	{
		return m_buffer;
	}

	bool IsValid() const;

	void CopyFromStaging( VkCommandBuffer commandBuffer );
	void ReleaseStaging( Device* device );

	uint32_t size() const;
	uint32_t stride() const;
	uint32_t offset() const;
	uint32_t index() const;

private:
	VkResult CreateBuffer( Device* device, BufferType type, const uint8_t* data );
	VkDeviceMemory m_memory;
	VkBuffer m_buffer;
	uint32_t m_size;
	uint32_t m_stride;
	uint32_t m_offset;
	uint32_t m_index;

	VkDeviceMemory m_stagingMemory;
	VkBuffer m_stagingBuffer;

	bool m_isValid;
};


namespace BufferBuilder
{
Buffer* Build( Device* device, const uint8_t* data, BufferType type, uint32_t size, uint32_t offset, uint32_t stride, uint32_t index );
}
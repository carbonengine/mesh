#pragma once
#include "device.h"
#include <cmf/cmf.h>
#include <vulkan/vulkan.h>
#include "rendering/renderer.h"

enum BufferType
{
	BufferTypeVertex,
	BufferTypeIndex
};


// Simple buffer object.
// Holds on to the memory and buffer
class Buffer
{
public:
	Buffer();
	~Buffer();

	void Release( const Renderer* renderer );
	VkResult Initialize( const Renderer* renderer, BufferType type, const uint8_t* data, uint32_t size, uint32_t stride );

	VkBuffer GetGpuBuffer() const
	{
		return m_buffer;
	}

	bool IsValid() const;

	void CopyFromStaging( VkCommandBuffer commandBuffer );
	void ReleaseStaging( const Renderer* renderer );

	uint32_t size() const;
	uint32_t stride() const;

private:
	VkResult CreateBuffer( const Renderer* renderer, BufferType type, const uint8_t* data );
	VkDeviceMemory m_memory;
	VkBuffer m_buffer;
	uint32_t m_size;
	uint32_t m_stride;

	VkDeviceMemory m_stagingMemory;
	VkBuffer m_stagingBuffer;

	bool m_isValid;
};


namespace BufferBuilder
{
Buffer* Build( const Renderer* renderer, const uint8_t* data, uint32_t size, BufferType type, uint32_t stride );
}
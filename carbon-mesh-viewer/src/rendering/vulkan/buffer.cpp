#include "buffer.h"
#include <rendering/renderer.h>
#include "vulkanerrors.h"


namespace BufferBuilder
{

Buffer* Build( const Renderer* renderer, const uint8_t* data, uint32_t size, BufferType type, uint32_t stride )
{
	Buffer* buffer = new Buffer();

	auto result = buffer->Initialize( renderer, type, data, size, stride );
	if( result != VK_SUCCESS )
	{
		delete buffer;
		return nullptr;
	}
	return buffer;
}

}

Buffer::Buffer() :
	m_isValid( false ),
	m_buffer( VK_NULL_HANDLE ),
	m_memory( VK_NULL_HANDLE ),
	m_stagingBuffer( VK_NULL_HANDLE ),
	m_stagingMemory( VK_NULL_HANDLE ),
	m_size( 0 ),
	m_stride( 0 )
{
}

Buffer::~Buffer()
{
}

VkResult Buffer::Initialize( const Renderer* renderer, BufferType type, const uint8_t* data, uint32_t size, uint32_t stride )
{
	m_size = size;
	m_stride = stride;

	VkBufferUsageFlags usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	if( type == BufferType::BufferTypeIndex )
	{
		usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	}
	auto result = CreateBuffer( renderer, type, data );
	if( result != VK_SUCCESS )
	{
        Log::Error( "Failed to create buffer.\n" );
		return result;
	}
	m_isValid = true;
	return VK_SUCCESS;
}

void Buffer::Release( const Renderer* renderer )
{
	ReleaseStaging( renderer );

	auto device = renderer->GetDevice();
	auto logicalDevice = device->GetLogicalDevice();
	auto allocator = renderer->GetAllocator();

	if( m_buffer != VK_NULL_HANDLE )
	{
		vkDestroyBuffer( logicalDevice, m_buffer, allocator );
	}
	if( m_memory != VK_NULL_HANDLE )
	{
		vkFreeMemory( logicalDevice, m_memory, allocator );
	}
}

bool Buffer::IsValid() const
{
	return m_buffer != VK_NULL_HANDLE && m_memory != VK_NULL_HANDLE && m_stagingBuffer == VK_NULL_HANDLE && m_stagingMemory == VK_NULL_HANDLE;
}

VkResult Buffer::CreateBuffer( const Renderer* renderer, BufferType type, const uint8_t* data )
{
	auto device = renderer->GetDevice();
	auto logicalDevice = device->GetLogicalDevice();

	void* mappedData;

	VkMemoryAllocateInfo memAlloc{};
	memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	VkMemoryRequirements memReqs;
	VkBufferCreateInfo bufferCreateInfo{};

	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = m_size;
	// Buffer is used as the copy source
	bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	// Create a host-visible buffer to copy the vertex data to (staging buffer)
	CR_RETURN( vkCreateBuffer( logicalDevice, &bufferCreateInfo, nullptr, &m_stagingBuffer ) );
	vkGetBufferMemoryRequirements( logicalDevice, m_stagingBuffer, &memReqs );
	memAlloc.allocationSize = memReqs.size;
	// Request a host visible memory type that can be used to copy our data to
	// Also request it to be coherent, so that writes are visible to the GPU right after unmapping the buffer
	memAlloc.memoryTypeIndex = device->GetMemoryTypeIndex( memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	CR_RETURN( vkAllocateMemory( logicalDevice, &memAlloc, nullptr, &m_stagingMemory ) );

	// Map and copy
	CR_RETURN( vkMapMemory( logicalDevice, m_stagingMemory, 0, memAlloc.allocationSize, 0, &mappedData ) );
	memcpy( mappedData, data, m_size );
	vkUnmapMemory( logicalDevice, m_stagingMemory );
	CR_RETURN( vkBindBufferMemory( logicalDevice, m_stagingBuffer, m_stagingMemory, 0 ) );

	if( type == BufferType::BufferTypeVertex )
	{
		bufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	}
	else if( type == BufferType::BufferTypeIndex )
	{
		bufferCreateInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	}
	else
	{
        Log::Error( "Invalid buffer type\n" );
		return VK_ERROR_UNKNOWN;
	}

	CR_RETURN( vkCreateBuffer( logicalDevice, &bufferCreateInfo, nullptr, &m_buffer ) );
	vkGetBufferMemoryRequirements( logicalDevice, m_buffer, &memReqs );
	memAlloc.allocationSize = memReqs.size;
	memAlloc.memoryTypeIndex = device->GetMemoryTypeIndex( memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	CR_RETURN( vkAllocateMemory( logicalDevice, &memAlloc, nullptr, &m_memory ) );
	CR_RETURN( vkBindBufferMemory( logicalDevice, m_buffer, m_memory, 0 ) );

	return VK_SUCCESS;
}

void Buffer::CopyFromStaging( VkCommandBuffer commandBuffer )
{
	VkBufferCopy copyRegion{};
	copyRegion.size = m_size;
	vkCmdCopyBuffer( commandBuffer, m_stagingBuffer, m_buffer, 1, &copyRegion );
}

void Buffer::ReleaseStaging( const Renderer* renderer )
{
	auto device = renderer->GetDevice();
	auto logicalDevice = device->GetLogicalDevice();
	if( m_stagingBuffer != VK_NULL_HANDLE )
	{
		vkDestroyBuffer( logicalDevice, m_stagingBuffer, nullptr );
		m_stagingBuffer = VK_NULL_HANDLE;
	}
	if( m_stagingMemory != VK_NULL_HANDLE )
	{
		vkFreeMemory( logicalDevice, m_stagingMemory, nullptr );
		m_stagingMemory = VK_NULL_HANDLE;
	}
}

uint32_t Buffer::stride() const
{
	return m_stride;
}

uint32_t Buffer::size() const
{
	return m_size;
}

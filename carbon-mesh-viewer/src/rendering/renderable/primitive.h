#pragma once

#include "../renderer.h"
#include "../vulkan/commandbuffer.h"
#include "../vulkan/shadercache.h"


class PrimitiveRenderable
{
public:
	PrimitiveRenderable( std::shared_ptr<const Renderer> renderer );
	~PrimitiveRenderable();

	void SetBufferData( const uint8_t* data, uint32_t size, uint32_t stride );
	void SetVertexDescriptions( const std::vector<VkVertexInputAttributeDescription>& vertexDescriptions );
	void SetLineWidth( float lineWidth );
	void SetTopology( VkPrimitiveTopology topology );

	VkResult Initialize();
	void Render( CommandBuffer& commandBuffer );
	VkResult SetRenderingMode( const ShaderCache* shaderCache, std::string shaderName, VkPolygonMode polygonMode );

private:
	std::shared_ptr<const Renderer> m_renderer{ nullptr };
    
    Buffer* m_vertexBuffer{ nullptr };

	std::vector<VkVertexInputAttributeDescription> m_vertexDescriptions{};
	uint32_t m_stride{ 0 };
	uint32_t m_size{ 0 };
	const uint8_t* m_data{ nullptr };
	VkPipeline m_pipeline{ VK_NULL_HANDLE };
	VkPrimitiveTopology m_topology{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };
	float m_lineWidth{ 1.0f };
};
#pragma once

#include "../renderer.h"
#include "../vulkan/buffer.h"
#include "../vulkan/commandbuffer.h"

class MeshLodRenderable
{
public:
	struct Area
	{
		uint32_t firstElement = 0;
		uint32_t elementCount = 0;
	};
	MeshLodRenderable( std::shared_ptr<const Renderer> renderer );
	MeshLodRenderable( const CmfContent* data, cmf::MeshLod cmfLod, std::shared_ptr<const Renderer> renderer );
	~MeshLodRenderable();

	void Initialize( VkCommandBuffer initializeCmd );
	void Finalize();
	void Render( CommandBuffer& commandBuffer, int32_t areaIndex = -1 ) const;

	void SetVertexData( const uint8_t* data, uint32_t size, uint32_t stride );
	void SetIndexData( const uint8_t* data, uint32_t size, uint32_t stride );
	void AddArea( uint32_t firstElement, uint32_t elementCount );

private:
	std::shared_ptr<const Renderer> m_renderer;

	Buffer* m_vertexBuffer{ nullptr };
	Buffer* m_indexBuffer{ nullptr };

	struct BufferData
	{
		const uint8_t* data{ nullptr };
		uint32_t size{ 0 };
		uint32_t stride{ 0 };
	};

	BufferData m_vertexData = {};
	BufferData m_indexData = {};

	std::vector<Area> m_areas;
};
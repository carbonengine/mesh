#pragma once

#include <vector>

#include "../vulkan/commandbuffer.h"
#include "../vulkan/shadercache.h"
#include "meshlod.h"

class MeshRenderable
{
public:
	MeshRenderable( std::shared_ptr<const Renderer> renderer );
	MeshRenderable( const CmfContent* data, const cmf::Mesh cmfMesh, std::shared_ptr<const Renderer> renderer );
	~MeshRenderable();

	void Initialize( VkCommandBuffer initializeCmd );
	void Finalize();

	void Render( CommandBuffer& commandBuffer, uint32_t lodIndex, int32_t areaIndex = -1 ) const;
	VkResult SetRenderingMode( const ShaderCache* shaderCache, std::string shaderName, VkPolygonMode polygonMode );

	void SetVertexDescriptions( const std::vector<VkVertexInputAttributeDescription>& vertexDescriptions );
	void AddLodRenderable( MeshLodRenderable&& lodRenderable );
	void SetStride( uint32_t stride );
	void SetTopology( VkPrimitiveTopology topology );
	void SetLineWidth( float lineWidth );

private:
	std::vector<MeshLodRenderable> m_lods;
	std::vector<VkVertexInputAttributeDescription> m_vertexDescriptions;
	std::shared_ptr<const Renderer> m_renderer;

	uint32_t m_stride{ 0 };
	VkPipeline m_pipeline{ VK_NULL_HANDLE };
	VkPrimitiveTopology m_topology{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };
	float m_lineWidth{ 1.0f };
};
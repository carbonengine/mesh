#pragma once

#include <vector>

#include "../vulkan/commandbuffer.h"
#include "../vulkan/shadercache.h"
#include "meshlod.h"

class MeshRenderable
{
public:
	MeshRenderable( CmfContent* data, const cmf::Mesh& cmfMesh, std::shared_ptr<const Renderer> renderer );
	~MeshRenderable();

	void Initialize( AppState& appState, VkCommandBuffer initializeCmd );
	void Finalize();

	void Render( CommandBuffer& commandBuffer, const AppState& appState, uint32_t lodIndex );
	VkResult SetRenderingMode( const ShaderCache* shaderCache, std::string shaderName, VkPolygonMode polygonMode );

private:
	std::vector<MeshLodRenderable> m_lods;
	std::vector<VkVertexInputAttributeDescription> m_vertexDescriptions;
	std::shared_ptr<const Renderer> m_renderer;

	uint32_t m_stride{ 0 };
	VkPipeline m_pipeline{ VK_NULL_HANDLE };
	VkPrimitiveTopology m_topology{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };
	bool m_display{ true };

	cmf::Mesh m_cmfMesh{};
	CmfContent* m_data{ nullptr };
};
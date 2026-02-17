#pragma once

#include <vector>

#include "../camera.h"
#include "../vulkan/commandbuffer.h"
#include "meshlod.h"
#include "primitive.h"

class MeshRenderable
{
public:
	MeshRenderable( CmfContent* data, const cmf::Mesh& cmfMesh, std::shared_ptr<const Renderer> renderer );
	~MeshRenderable();

	void Initialize( AppState& appState, VkCommandBuffer initializeCmd );
	void Finalize();

	void Render( CommandBuffer& commandBuffer, const AppState& appState, const Camera& camera, uint32_t lodIndex );

	VkResult SetRenderingMode( std::string shaderName, VkPolygonMode polygonMode );

private:
	std::vector<MeshLodRenderable> m_lods;
	std::vector<VkVertexInputAttributeDescription> m_vertexDescriptions;
	std::shared_ptr<const Renderer> m_renderer;

	struct VertexUboData
	{
		Matrix proj;
		Matrix view;
	};

	uint32_t m_stride{ 0 };
	VkPrimitiveTopology m_topology{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };
	bool m_display{ true };
	bool m_wireframe{ false };
	VkPolygonMode m_polygonMode{ VK_POLYGON_MODE_FILL };

	Effect m_modelEffect;
	Effect m_wireframeEffect;

	bool m_showBoundingBox{ false };
	PrimitiveRenderable m_boundingBox;
	Matrix m_boundingBoxTransform{};

	cmf::Mesh m_cmfMesh{};
};
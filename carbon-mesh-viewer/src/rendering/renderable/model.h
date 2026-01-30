#pragma once

#include "../renderer.h"
#include "../vulkan/commandbuffer.h"
#include "../vulkan/shadercache.h"
#include "mesh.h"


class ModelRenderable
{
public:
	ModelRenderable( std::shared_ptr<const Renderer> renderer );
	ModelRenderable( const CmfContent* data, std::shared_ptr<const Renderer> renderer );
	~ModelRenderable();

	VkResult Initialize();
	void RenderMesh( CommandBuffer& commandBuffer, uint32_t lod, int32_t meshIndex = -1, int32_t areaIndex = -1 );
	VkResult SetRenderingMode( const ShaderCache* shaderCache, std::string shaderName, VkPolygonMode polygonMode );

	void AddMeshRenderable( MeshRenderable&& meshRenderable );

private:
	std::vector<MeshRenderable> m_meshes{};
	std::shared_ptr<const Renderer> m_renderer{ nullptr };
};
#pragma once

#include "../renderer.h"
#include "../vulkan/commandbuffer.h"
#include "../vulkan/shadercache.h"
#include "mesh.h"


class ModelRenderable
{
public:
	ModelRenderable( CmfContent* data, std::shared_ptr<const Renderer> renderer );
	~ModelRenderable();

	VkResult Initialize( AppState& appState );
	void RenderMesh( CommandBuffer& commandBuffer, const AppState& appState );
	VkResult SetRenderingMode( const ShaderCache* shaderCache, std::string shaderName, VkPolygonMode polygonMode );

private:
	std::vector<MeshRenderable> m_meshes{};
	std::shared_ptr<const Renderer> m_renderer{ nullptr };
};
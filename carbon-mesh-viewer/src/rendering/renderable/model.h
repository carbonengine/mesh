#pragma once

#include "../renderer.h"
#include "../vulkan/commandbuffer.h"
#include "mesh.h"


class ModelRenderable
{
public:
	ModelRenderable( std::shared_ptr<CmfContent> data, std::shared_ptr<const Renderer> renderer );
	~ModelRenderable();

	VkResult Initialize( AppState& appState );
	VkResult PrepareModel( ComputeCommandBuffer& computeCommandBuffer );

	void RenderMesh( GraphicsCommandBuffer& commandBuffer, const AppState& appState, const Camera& camera );
	VkResult SetRenderingMode( std::string shaderName, VkPolygonMode polygonMode );

private:
	void SetAnimation( std::string animationName );

	std::vector<MeshRenderable> m_meshes{};
	std::shared_ptr<const Renderer> m_renderer{ nullptr };

	bool m_showBoundingBox{ false };
	PrimitiveRenderable m_boundingBox;
	Matrix m_boundingBoxTransform{};
	std::vector<Matrix> m_boneMatrices{};
};
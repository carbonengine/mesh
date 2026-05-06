#pragma once

#include <vector>
#include <cmf/animation.h>

#include "../camera.h"
#include "../vulkan/commandbuffer.h"
#include "../vulkan/graphicseffect.h"
#include "geometryprepass.h"
#include "primitive.h"

class MeshRenderable
{
public:
	MeshRenderable( std::shared_ptr<CmfContent> data, const cmf::Mesh& cmfMesh, std::shared_ptr<const Renderer> renderer );

	void Initialize( AppState& appState );
	void Render( GraphicsCommandBuffer& commandBuffer, const AppState& appState, const Camera& camera );
	void PrepareMesh( ComputeCommandBuffer& computeCommandBuffer );
	VkResult SetRenderingMode( std::string shaderName, VkPolygonMode polygonMode );

private:
	void InitializeAnimationData( std::shared_ptr<CmfContent> data );
	void Draw( GraphicsCommandBuffer& commandBuffer );
	void DrawIndexed( GraphicsCommandBuffer& commandBuffer );
	void SetLod( uint32_t lodLevel );
	void SetAnimation( std::string animationName );

	struct Area
	{
		uint32_t firstElement = 0;
		uint32_t elementCount = 0;
	};
	GraphicsEffect GetAudioOcclusionEffect( std::shared_ptr<const Renderer> renderer, const cmf::Mesh& cmfMesh );

	std::vector<VkVertexInputAttributeDescription> m_vertexDescriptions;

	struct VertexUboData
	{
		Matrix proj;
		Matrix view;
	};

	std::vector<cmf::VertexElement> m_availableVertexElements;
	std::shared_ptr<const Renderer> m_renderer;

	uint32_t m_stride{ 0 };
	VkPolygonMode m_polygonMode{ VK_POLYGON_MODE_FILL };
	VkPrimitiveTopology m_topology{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };
	std::string m_shaderName{ "" };

	bool m_display{ true };
	bool m_wireframe{ false };

	// effects
	GraphicsEffect m_modelEffect;
	GraphicsEffect m_wireframeEffect;

	// geometry prepass
	GeometryPrePass m_prepass;

	// bounding box
	bool m_showBoundingBox{ false };
	PrimitiveRenderable m_boundingBox;
	Matrix m_boundingBoxTransform{};

	bool m_audioOcclusion{ false };
	PrimitiveRenderable m_audioOcclusionRenderable;

	cmf::Mesh m_cmfMesh{};
	std::vector<Area> m_areas{};

	std::unique_ptr<cmf::AnimationPlayer> m_animationPlayer = nullptr;
	std::unique_ptr<cmf::Animation> m_currentAnimation = nullptr;
	cmf::Span<cmf::Animation> m_animations;
	std::unique_ptr<cmf::Skeleton> m_baseSkeleton = nullptr;
	std::unique_ptr<cmf::Skeleton> m_animationSkeleton = nullptr;
	cmf::SkeletonPose m_currentPose{};
	std::vector<std::pair<uint32_t, uint32_t>> m_morphCurveToTargetMapping{};
};
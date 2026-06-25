// Copyright © 2026 CCP ehf.

#pragma once

#include <vector>
#include <cmf/animation.h>
#include <limits>

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
	void Update( AppState& appState, const Camera& camera );
	void Render( GraphicsCommandBuffer& commandBuffer, const AppState& appState, const Camera& camera );
	void RenderDebug( GraphicsCommandBuffer& commandBuffer, const AppState& appState, const Camera& camera );
	void PrepareMesh( ComputeCommandBuffer& computeCommandBuffer );
	VkResult SetRenderingMode( std::string shaderName, GraphicsEffectTypes::ShaderInputDeclaration shaderInputDeclaration, VkPolygonMode polygonMode );

	void SetAnimation( const cmf::Animation* animation );
	void UpdateMeshCurves( float animationTime, const cmf::Animation* animation, AppState& appState );
	void SetSkeletonPose( const std::array<Matrix, 0xFF>& boneTransforms );
	uint8_t GetSkeletonIndex() const;

private:
	void Draw( GraphicsCommandBuffer& commandBuffer );
	void DrawIndexed( GraphicsCommandBuffer& commandBuffer );
	void SetLod( uint32_t lodLevel );

	struct Area
	{
		uint32_t firstElement = 0;
		uint32_t elementCount = 0;
	};
	GraphicsEffect GetAudioOcclusionEffect( std::shared_ptr<const Renderer> renderer, const cmf::Mesh& cmfMesh );

	std::vector<cmf::VertexElement> m_availableVertexElements;
	std::shared_ptr<const Renderer> m_renderer;

	uint32_t m_stride{ 0 };
	uint32_t m_currentLod{ std::numeric_limits<uint32_t>::max() };
	size_t m_meshIndex{ 0 };
	CcpMath::Sphere m_boundingSphere{};
	VkPolygonMode m_polygonMode{ VK_POLYGON_MODE_FILL };
	VkPrimitiveTopology m_topology{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };
	std::string m_shaderName{ "" };

	bool m_display{ true };
	bool m_wireframe{ false };
	bool m_audioOcclusion{ false };
	bool m_showBoundingBox{ false };
	bool m_initialized{ false };

	// effects
	GraphicsEffect m_modelEffect;
	GraphicsEffect m_wireframeEffect;

	// geometry prepass
	GeometryPrePass m_prepass;

	cmf::Mesh m_cmfMesh{};
	std::vector<Area> m_areas{};

	std::vector<std::pair<uint32_t, uint32_t>> m_morphCurveToTargetMapping{};
	std::vector<size_t> m_boneBindingToBoneIndexMapping{};

	// debug renderables
	// bounding box
	PrimitiveRenderable m_boundingBox;
	Matrix m_boundingBoxTransform{};

	std::vector<PrimitiveRenderable> m_normalAxisRenderables;
	std::vector<PrimitiveRenderable> m_tangentAxisRenderables;
	std::vector<PrimitiveRenderable> m_binormalAxisRenderables;

	// audio occlusion
	PrimitiveRenderable m_audioOcclusionRenderable;
};
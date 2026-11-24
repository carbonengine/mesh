// Copyright © 2026 CCP ehf.

#pragma once

#include <functional>
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

	/// Called for every morph weight driven by an animation curve: (morph index, weight)
	using MorphWeightCallback = std::function<void( uint32_t, float )>;

	void Initialize();
	/// Picks the LOD from the size on screen when autoLod is true
	void Update( const Camera& camera, bool autoLod );
	void Render( GraphicsCommandBuffer& commandBuffer, const Camera& camera );
	void RenderDebug( GraphicsCommandBuffer& commandBuffer, const Camera& camera );
	void PrepareMesh( ComputeCommandBuffer& computeCommandBuffer );
	VkResult SetRenderingMode( std::string shaderName, GraphicsEffectTypes::ShaderInputDeclaration shaderInputDeclaration );

	void SetAnimation( const cmf::Animation* animation );
	void UpdateMeshCurves( float animationTime, const cmf::Animation* animation, const MorphWeightCallback& onMorphWeight );
	void SetSkeletonPose( const std::array<Matrix, 0xFF>& boneTransforms );
	uint8_t GetSkeletonIndex() const;

	void SetLod( uint32_t lodLevel );
	[[nodiscard]] uint32_t GetCurrentLod() const;
	[[nodiscard]] float GetScreenSize() const;

	void SetVisible( bool visible );
	void SetAreaVisible( size_t areaIndex, bool visible );
	void SetWireframe( bool enabled );
	void SetAudioOcclusion( bool enabled );
	void SetShowBoundingBox( bool enabled );
	void SetMorphWeight( uint32_t morphIndex, float weight );

	/// The vertex element usage indices of the normal/tangent/binormal debug axes, in the order used by SetShow* functions
	[[nodiscard]] const std::vector<uint32_t>& GetNormalAxisUsages() const;
	[[nodiscard]] const std::vector<uint32_t>& GetTangentAxisUsages() const;
	[[nodiscard]] const std::vector<uint32_t>& GetBinormalAxisUsages() const;
	void SetShowNormals( size_t index, bool enabled );
	void SetShowTangents( size_t index, bool enabled );
	void SetShowBinormals( size_t index, bool enabled );

	[[nodiscard]] const cmf::Mesh& GetCmfMesh() const;

private:
	void Draw( GraphicsCommandBuffer& commandBuffer );
	void DrawIndexed( GraphicsCommandBuffer& commandBuffer );
	bool IsAreaVisible( size_t areaIndex ) const;

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
	float m_screenSize{ 0.0f };
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
	std::vector<bool> m_areaDisplay{};

	std::vector<std::pair<uint32_t, uint32_t>> m_morphCurveToTargetMapping{};
	std::vector<size_t> m_boneBindingToBoneIndexMapping{};

	// debug renderables
	// bounding box
	PrimitiveRenderable m_boundingBox;
	Matrix m_boundingBoxTransform{};

	std::vector<PrimitiveRenderable> m_normalAxisRenderables;
	std::vector<PrimitiveRenderable> m_tangentAxisRenderables;
	std::vector<PrimitiveRenderable> m_binormalAxisRenderables;
	std::vector<uint32_t> m_normalAxisUsages;
	std::vector<uint32_t> m_tangentAxisUsages;
	std::vector<uint32_t> m_binormalAxisUsages;
	std::vector<bool> m_showNormals;
	std::vector<bool> m_showTangents;
	std::vector<bool> m_showBinormals;

	// audio occlusion
	PrimitiveRenderable m_audioOcclusionRenderable;
};
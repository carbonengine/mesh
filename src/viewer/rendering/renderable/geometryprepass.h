// Copyright © 2026 CCP ehf.

#pragma once

#include "../renderer.h"
#include "../vulkan/buffer.h"
#include "../vulkan/commandbuffer.h"
#include "../vulkan/computeeffect.h"
#include <cmf/animation.h>
#include <string>

/// <summary>
/// This class handles a geometry prepass for a cmf mesh.
///
/// The geometry prepass consists of applying morph targets and bone animations.
///
/// Since a generalized vertex format is supported each vertex attribute is morphed separatly
///
/// </summary>
class GeometryPrePass
{
public:
	GeometryPrePass( std::shared_ptr<const Renderer> renderer, std::shared_ptr<CmfContent> cmfContent, const cmf::Mesh& mesh );
	~GeometryPrePass();

	void Initialize( AppState& appState );
	void Process( ComputeCommandBuffer& commandBuffer );
	const Buffer& GetIndexBuffer() const;
	const Buffer& GetVertexBuffer() const;

	void SetMorphWeight( size_t morphIndex, float weight );
	void SetSkeletonPose( const std::array<Matrix, 0xFF>& boneTransforms );

	void SetLod( uint32_t lod );

private:
	void SetupForDynamicMesh( const cmf::MeshLod& lod );
	void SetupForStaticMesh( const cmf::MeshLod& lod );


	void IssueBarrier( VkCommandBuffer& commandBuffer, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask );

	struct GeoPrepassUBO
	{
		uint32_t morphJobCount; // number of morph jobs
		uint32_t vertexCount; // number of vertices in the vertex buffer
		uint32_t vertexStride; // stride of the vertex buffer
		uint32_t morphBufferCount; // number of morph targets
		uint32_t morphBufferSize; // single morph buffer size
		uint32_t morphVertexStride; // single morph vertex stride
		uint32_t boneCount; // number of bones in the skeleton
		uint32_t boneIndexElementIndex; // index into the vertex declaration for the bone index attribute
		uint32_t boneWeightElementIndex; // index into the vertex declaration for the bone weight attribute
		uint32_t sourceElementCount; //  number of source elements in the source elements array
		uint32_t skinnedElementCount; // number of skinned elements in the skinned elements array
	};

	// One job defines a morph operation for a single vertex attribute, for example position or normal
	// it will go through all the source buffer and apply the morph targets to the output buffer
	struct MorphJob
	{
		uint32_t sourceElementIndex; // index into the source vertex declaration for this morph job
		uint32_t morphElementIndex; // index into the morph target vertex declaration for this morph job
	};

	struct Element
	{
		uint32_t type;
		uint32_t elementCount;
		uint32_t offset;
		uint32_t normalized;
		uint32_t usage; // vertex attribute usage, for example position or normal
	};

	const cmf::Mesh m_cmfMesh;
	std::shared_ptr<CmfContent> m_cmfContent;
	std::shared_ptr<const Renderer> m_renderer;

	GeoPrepassUBO m_ubo{};
	std::vector<float> m_weights{};
	std::vector<MorphJob> m_morphJobs{};

	Buffer m_indexBuffer{};
	Buffer m_vertexBuffer{};
	Buffer m_computeOutBuffer{}; // the output of the compute shader
	Buffer m_morphTargetBuffer{}; // contains all the morph target data, sequentially
	Buffer m_weightBuffer{};
	Buffer m_morphJobBuffer{};
	Buffer m_elementBuffer{};
	Buffer m_skinnedElementBuffer{};
	Buffer m_boneTransformBuffer{};

	uint32_t m_boneCount{ 0 };
	uint32_t m_boneIndexElementIndex{ std::numeric_limits<uint32_t>::max() };
	uint32_t m_boneWeightElementIndex{ std::numeric_limits<uint32_t>::max() };
	uint32_t m_positionElementIndex{ std::numeric_limits<uint32_t>::max() };

	ComputeEffect m_effect;
	uint32_t m_currentLod{ std::numeric_limits<uint32_t>::max() };
	std::array<Matrix, 0xFF> m_boneTransforms{};

	bool m_isDynamic{ false };
	bool m_hasBoneIndices{ false };
};
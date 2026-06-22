// Copyright © 2026 CCP ehf.

#pragma once

#include "../appState.h"

class UIDetailWindow
{
public:
	void Render( AppState& appState, float marginTop, float marginBottom );

private:
	struct SelectedItem
	{
		enum Type
		{
			None,
			SkeletonBones,
			BoneBindings,
			VertexBuffer,
			IndexBuffer,
			AudioOcclusionMesh,
			Animation,
			AnimationCurve
		};
		Type type = None;
		const void* context = nullptr;
		std::vector<uint32_t> selectedIndices;
		bool scrollTo = false;
	};

	void RenderFileHierarchy( const CmfContent& cmfContent );
	void RenderVertexData( const CmfContent& cmfContent, const cmf::Span<cmf::VertexElement>& decl, const cmf::BufferView& vb );
	void RenderAttributeTable( const uint8_t* vbData, uint32_t vertexCount, uint32_t stride, const std::vector<cmf::VertexElement>& attributes );
	void RenderIndexData( const CmfContent& cmfContent, const cmf::Mesh& mesh, const cmf::MeshLod& lod );
	void RenderSkeleton( const cmf::Skeleton& skeleton );
	void RenderBoneBindings( const CmfContent& cmfContent, const cmf::Mesh& mesh );
	void RenderAnimationChannels( const cmf::Animation& anim, const cmf::Data& data );
	void RenderAnimationCurves( const cmf::AnimationCurve& curve, const cmf::Animation& anim );
	void RenderAudioOccluder( const cmf::AudioOcclusionMesh& audioOcclusionMesh );

	void UpdateSelection( AppState& appState );

	SelectedItem m_selectedItem{};
	std::vector<std::pair<cmf::Usage, uint8_t>> m_hiddenAttributes;
	std::vector<std::pair<std::string, bool>> m_boneColumnFilter = { { "Name", true }, { "Parent", true }, { "Position", true }, { "Rotation", true }, { "Scale", true } };
};

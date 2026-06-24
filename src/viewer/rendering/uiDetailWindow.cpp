// Copyright © 2026 CCP ehf.

#include "uiDetailWindow.h"
#include "cmf/bufferstreams.h"
#include "cmf/declutils.h"
#include <imgui.h>
#include <filesystem>
#include <numeric>


namespace
{

constexpr uint8_t NO_SKELETON = 0xff;

const char* GetAnimationTargetTypeName( cmf::AnimationChannelTargetType type )
{
	switch( type )
	{
	case cmf::AnimationChannelTargetType::BonePosition:
		return "Bone Position";
	case cmf::AnimationChannelTargetType::BoneRotation:
		return "Bone Rotation";
	case cmf::AnimationChannelTargetType::BoneScale:
		return "Bone Scale";
	case cmf::AnimationChannelTargetType::MorphTarget:
		return "Morph Target";
	case cmf::AnimationChannelTargetType::Other:
		return "Other";
	}
	return "Unknown";
}

std::string GetUsageFlagLabel( cmf::Usage usage, uint8_t usageIndex )
{
	std::string name;
	switch( usage )
	{
	case cmf::Usage::Position:
		name = "Position";
		break;
	case cmf::Usage::Normal:
		name = "Normal";
		break;
	case cmf::Usage::Tangent:
		name = "Tangent";
		break;
	case cmf::Usage::Binormal:
		name = "Binormal";
		break;
	case cmf::Usage::TexCoord:
		name = "TexCoord";
		break;
	case cmf::Usage::Color:
		name = "Color";
		break;
	case cmf::Usage::BoneIndices:
		name = "Bone Indices";
		break;
	case cmf::Usage::BoneWeights:
		name = "Bone Weights";
		break;
	case cmf::Usage::PackedTangent:
		name = "Packed Tangent";
		break;
	case cmf::Usage::PackedTangentLegacy:
		name = "Packed Tangent (Legacy)";
		break;
	default:
		name = "Unknown";
		break;
	}
	if( usageIndex > 0 )
	{
		name += std::to_string( usageIndex );
	}
	return name;
}

const char* GetElementTypeName( cmf::ElementType type )
{
	switch( type )
	{
	case cmf::ElementType::Float32:
		return "Float32";
	case cmf::ElementType::Float16:
		return "Float16";
	case cmf::ElementType::UInt16Norm:
		return "UInt16Norm";
	case cmf::ElementType::UInt16:
		return "UInt16";
	case cmf::ElementType::Int16Norm:
		return "Int16Norm";
	case cmf::ElementType::Int16:
		return "Int16";
	case cmf::ElementType::UInt8Norm:
		return "UInt8Norm";
	case cmf::ElementType::UInt8:
		return "UInt8";
	case cmf::ElementType::Int8Norm:
		return "Int8Norm";
	case cmf::ElementType::Int8:
		return "Int8";
	}
	return "Unknown";
}

}

// ImGui is using a lot of variadic functions for text formatting, so we disable the cppcoreguidelines-pro-type-vararg lint for this file
// NOLINTBEGIN(cppcoreguidelines-pro-type-vararg)



void UIDetailWindow::Render( AppState& appState, float marginTop, float marginBottom )
{
	const float width = (float)appState.windowSize.GetValue().first;
	const float height = (float)appState.windowSize.GetValue().second;

	const float ySize = std::max( 1.0f, height - marginTop - marginBottom + 1 ); // +1 so we get an overlap of the borders

	// Pivot (1,0) anchors the top-right corner to the right edge of the viewport
	ImGui::SetNextWindowPos( ImVec2( width, marginTop ), ImGuiCond_Always, ImVec2( 1.0f, 0.0f ) );
	ImGui::SetNextWindowSizeConstraints( ImVec2( 0, ySize ), ImVec2( width, ySize ) );
	ImGui::SetNextWindowSize( ImVec2( width / 4.0f, ySize ), ImGuiCond_FirstUseEver );
	ImGui::SetNextWindowCollapsed( true, ImGuiCond_FirstUseEver );
	if( !ImGui::Begin( "Details", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings ) )
	{
		ImGui::End();
		return;
	}

	auto* cmfContent = appState.cmfContent.GetValue().get();
	if( cmfContent == nullptr || cmfContent->m_cmfData == nullptr )
	{
		ImGui::Text( "No CMF loaded" );
		ImGui::End();
		return;
	}

	if( !ImGui::BeginChild( "Hierarchy", ImVec2( 0, 0 ), ImGuiChildFlags_ResizeY ) )
	{
		ImGui::EndChild();
		ImGui::End();
		return;
	}
	RenderFileHierarchy( *cmfContent );
	for( auto& animSource : appState.modelState.animationOverrides )
	{
		RenderFileHierarchy( *animSource.GetValue() );
	}
	ImGui::EndChild();

	auto renderVertexBuffer = [&]( const void* vb, const CmfContent& cmfContent ) {
		for( auto& mesh : cmfContent.m_cmfData->meshes )
		{
			for( auto& lod : mesh.lods )
			{
				if( &lod.vb == vb )
				{
					RenderVertexData( cmfContent, mesh.decl, lod.vb );
					return true;
				}
				for( auto& morphTarget : lod.morphTargets )
				{
					if( &morphTarget.vb == vb )
					{
						RenderVertexData( cmfContent, mesh.morphTargets.decl, morphTarget.vb );
						return true;
					}
				}
			}
		}
		return false;
	};
	auto renderIndexBuffer = [&]( const void* ib, const CmfContent& cmfContent ) {
		for( auto& mesh : cmfContent.m_cmfData->meshes )
		{
			for( auto& lod : mesh.lods )
			{
				if( &lod.ib == ib )
				{
					RenderIndexData( cmfContent, mesh, lod );
					return true;
				}
			}
		}
		return false;
	};
	auto renderSkeleton = [&]( const void* skeletonPtr, const CmfContent& cmfContent ) {
		for( auto& skeleton : cmfContent.m_cmfData->skeletons )
		{
			if( &skeleton == skeletonPtr )
			{
				RenderSkeleton( skeleton );
				return true;
			}
		}
		return false;
	};
	auto renderBoneBindings = [&]( const void* boneBindingsPtr, const CmfContent& cmfContent ) {
		for( auto& mesh : cmfContent.m_cmfData->meshes )
		{
			if( &mesh.boneBindings == boneBindingsPtr )
			{
				RenderBoneBindings( cmfContent, mesh );
				return true;
			}
		}
		return false;
	};
	auto renderAnimation = [&]( const void* animationPtr, const CmfContent& cmfContent ) {
		for( auto& animation : cmfContent.m_cmfData->animations )
		{
			if( &animation == animationPtr )
			{
				RenderAnimationChannels( animation, *cmfContent.m_cmfData );
				return true;
			}
		}
		return false;
	};
	auto renderAnimationCurve = [&]( const void* curvePtr, const CmfContent& cmfContent ) {
		for( auto& animation : cmfContent.m_cmfData->animations )
		{
			for( auto& curve : animation.curves )
			{
				if( &curve == curvePtr )
				{
					RenderAnimationCurves( curve, animation );
					return true;
				}
			}
		}
		return false;
	};
	auto renderAudioOcclusionMesh = [&]( const void* audioOcclusionMeshPtr, const CmfContent& cmfContent ) {
		for( auto& mesh : cmfContent.m_cmfData->meshes )
		{
			if( &mesh.audioOcclusionMesh == audioOcclusionMeshPtr )
			{
				RenderAudioOccluder( mesh.audioOcclusionMesh );
				return true;
			}
		}
		return false;
	};

	auto renderForDataSources = [&]( const void* context, const auto& render ) {
		if( !render( context, *cmfContent ) )
		{
			for( const auto& animSource : appState.modelState.animationOverrides )
			{
				if( render( context, *animSource.GetValue() ) )
				{
					return;
				}
			}
		}
	};


	switch( m_selectedItem.type )
	{
	case SelectedItem::VertexBuffer:
		renderForDataSources( m_selectedItem.context, renderVertexBuffer );
		break;
	case SelectedItem::IndexBuffer:
		renderForDataSources( m_selectedItem.context, renderIndexBuffer );
		break;
	case SelectedItem::SkeletonBones:
		renderForDataSources( m_selectedItem.context, renderSkeleton );
		break;
	case SelectedItem::BoneBindings:
		renderForDataSources( m_selectedItem.context, renderBoneBindings );
		break;
	case SelectedItem::Animation:
		renderForDataSources( m_selectedItem.context, renderAnimation );
		break;
	case SelectedItem::AnimationCurve:
		renderForDataSources( m_selectedItem.context, renderAnimationCurve );
		break;
	case SelectedItem::AudioOcclusionMesh:
		renderForDataSources( m_selectedItem.context, renderAudioOcclusionMesh );
		break;
	default:
		break;
	}

	UpdateSelection( appState );

	ImGui::End();
}

void UIDetailWindow::UpdateSelection( AppState& appState )
{
	if( m_selectedItem.type == SelectedItem::SkeletonBones )
	{
		bool same = appState.modelState.selectedBones.size() == m_selectedItem.selectedIndices.size();
		if( same )
		{
			for( size_t i = 0; i < appState.modelState.selectedBones.size(); ++i )
			{
				if( appState.modelState.selectedBones[i].GetValue() != m_selectedItem.selectedIndices[i] )
				{
					same = false;
					break;
				}
			}
		}
		if( !same )
		{
			appState.modelState.selectedBones.Clear();
			for( const auto& boneIndex : m_selectedItem.selectedIndices )
			{
				appState.modelState.selectedBones.AddState( boneIndex );
			}
		}
	}
	else
	{
		appState.modelState.selectedBones.Clear();
	}
}

void UIDetailWindow::RenderFileHierarchy( const CmfContent& cmfContent )
{
	auto clickableNode = [&]( SelectedItem::Type type, const auto& context, const char* label ) {
		ImGui::PushID( &context );
		ImGui::PushStyleColor( ImGuiCol_HeaderHovered, ImVec4( 0.0f, 0.0f, 0.0f, 0.0f ) );
		ImGui::Indent( ImGui::GetStyle().IndentSpacing );
		ImGui::PushStyleVarX( ImGuiStyleVar_ItemSpacing, 0.0f );
		ImGui::Selectable( "", m_selectedItem.type == type && m_selectedItem.context == &context, ImGuiSelectableFlags_AllowOverlap );
		ImGui::SameLine();
		if( ImGui::TextLink( label ) )
		{
			m_selectedItem = SelectedItem{ type, &context };
		}
		ImGui::Unindent( ImGui::GetStyle().IndentSpacing );
		ImGui::PopStyleVar();
		ImGui::PopStyleColor();
		ImGui::PopID();
	};
	auto textNode = [&]( const char* label, auto... vars ) {
		ImGui::Indent( ImGui::GetStyle().IndentSpacing );
		ImGui::Text( label, vars... );
		ImGui::Unindent( ImGui::GetStyle().IndentSpacing );
	};

	const std::filesystem::path filePath( cmfContent.m_filePath );
	if( ImGui::TreeNodeEx( &cmfContent, ImGuiTreeNodeFlags_DefaultOpen, "%s", filePath.filename().string().c_str() ) )
	{
		const auto& cmfData = *cmfContent.m_cmfData;

		auto renderSkeleton = [&]( const cmf::Skeleton& skeleton ) {
			const auto name = "Skeleton[" + std::to_string( std::distance( cmfData.skeletons.begin(), &skeleton ) ) + "]: " + cmf::ToStdString( skeleton.name ) + " (" + std::to_string( skeleton.bones.size() ) + " bones)";
			clickableNode( SelectedItem::Type::SkeletonBones, skeleton, name.c_str() );
		};

		auto renderMesh = [&]( const cmf::Mesh& mesh ) {
			if( ImGui::TreeNode( &mesh, "Mesh: %s", cmf::ToStdString( mesh.name ).c_str() ) )
			{
				if( !mesh.lods.empty() )
				{
					if( ImGui::TreeNode( &mesh.lods, "LODs (%zu)", mesh.lods.size() ) )
					{
						for( const auto& lod : mesh.lods )
						{
							std::string lodLabel = "LOD " + std::to_string( std::distance( mesh.lods.begin(), &lod ) );
							if( lod.threshold == cmf::MeshLod::MAX_THRESHOLD )
							{
								lodLabel += " (base)";
							}
							else
							{
								lodLabel += " (threshold: " + std::to_string( lod.threshold ) + "px)";
							}
							if( ImGui::TreeNode( &lod, "%s", lodLabel.c_str() ) )
							{
								const uint32_t vertexCount = lod.vb.stride > 0 ? lod.vb.size / lod.vb.stride : 0;
								const uint32_t indexCount = lod.ib.stride > 0 ? lod.ib.size / lod.ib.stride : 0;

								clickableNode( SelectedItem::Type::VertexBuffer, lod.vb, std::string( "Vertices: " + std::to_string( vertexCount ) ).c_str() );
								clickableNode( SelectedItem::Type::IndexBuffer, lod.ib, std::string( "Indices: " + std::to_string( indexCount ) ).c_str() );
								ImGui::TreePop();
							}
						}
						ImGui::TreePop();
					}
				}
				if( !mesh.areas.empty() )
				{
					if( ImGui::TreeNode( &mesh.areas, "Mesh Areas (%zu)", mesh.areas.size() ) )
					{
						for( const auto& area : mesh.areas )
						{
							if( ImGui::TreeNode( &area, "%s", area.name.empty() ? "Unnamed Area" : cmf::ToStdString( area.name ).c_str() ) )
							{
								textNode( "Affected by Bones: %s", area.affectedByBones ? "Yes" : "No" );
								textNode( "Affected by Morphs: %s", area.affectedByMorphTargets ? "Yes" : "No" );
								if( !area.bones.empty() )
								{
									if( ImGui::TreeNode( &area.bones, "Bones" ) )
									{
										for( const auto& boneIndex : area.bones )
										{
											const auto& binding = mesh.boneBindings[boneIndex];
											ImGui::PushID( int( boneIndex ) );
											clickableNode( SelectedItem::Type::BoneBindings, mesh.boneBindings, ToStdString( binding.name ).c_str() );
											ImGui::PopID();
											textNode(
												"Bounds: [%.4f, %.4f, %.4f] - [%.4f, %.4f, %.4f]",
												binding.bounds.m_min.x,
												binding.bounds.m_min.y,
												binding.bounds.m_min.z,
												binding.bounds.m_max.x,
												binding.bounds.m_max.y,
												binding.bounds.m_max.z );
										}
										ImGui::TreePop();
									}
								}
								ImGui::TreePop();
							}
						}
						ImGui::TreePop();
					}
				}
				if( !mesh.boneBindings.empty() )
				{
					clickableNode( SelectedItem::Type::BoneBindings, mesh.boneBindings, "Bone Bindings" );
				}
				if( !mesh.morphTargets.targets.empty() )
				{
					if( ImGui::TreeNode( &mesh.morphTargets.targets, "Morph Targets (%zu)", mesh.morphTargets.targets.size() ) )
					{
						for( size_t ti = 0; ti < mesh.morphTargets.targets.size(); ++ti )
						{
							const auto& target = mesh.morphTargets.targets[ti];
							if( ImGui::TreeNode( &target, "%s", cmf::ToStdString( target.name ).c_str() ) )
							{
								textNode( "Max Displacement: %.4f", target.maxDisplacement );
								for( const auto& lod : mesh.lods )
								{
									if( ti < lod.morphTargets.size() )
									{
										const auto& morphLod = lod.morphTargets[ti];
										const uint32_t vtxCount = morphLod.vb.stride > 0 ? morphLod.vb.size / morphLod.vb.stride : 0;
										clickableNode( SelectedItem::VertexBuffer, morphLod.vb, std::string( "LOD " + std::to_string( std::distance( mesh.lods.begin(), &lod ) ) + ": " + std::to_string( vtxCount ) + " vertices" ).c_str() );
									}
								}
								ImGui::TreePop();
							}
						}
						ImGui::TreePop();
					}
				}
				textNode( "Bounds Min:  %.4f  %.4f  %.4f", mesh.bounds.m_min.x, mesh.bounds.m_min.y, mesh.bounds.m_min.z );
				textNode( "Bounds Max:  %.4f  %.4f  %.4f", mesh.bounds.m_max.x, mesh.bounds.m_max.y, mesh.bounds.m_max.z );
				textNode( "Bounds Size: %.4f  %.4f  %.4f", mesh.bounds.Size().x, mesh.bounds.Size().y, mesh.bounds.Size().z );

				if( mesh.skeleton != NO_SKELETON && size_t( mesh.skeleton ) < cmfData.skeletons.size() )
				{
					const auto& skeleton = cmfData.skeletons[mesh.skeleton];
					renderSkeleton( skeleton );
				}
				if( !mesh.audioOcclusionMesh.vertices.empty() )
				{
					clickableNode( SelectedItem::Type::AudioOcclusionMesh, mesh.audioOcclusionMesh, "Audio Occlusion Mesh" );
				}
				ImGui::TreePop();
			}
		};

		if( !cmfData.meshes.empty() && ImGui::TreeNodeEx( &cmfData.meshes, ImGuiTreeNodeFlags_DefaultOpen, "Meshes (%zu)", cmfData.meshes.size() ) )
		{
			for( const auto& mesh : cmfData.meshes )
			{
				renderMesh( mesh );
			}
			ImGui::TreePop(); // Meshes
		}

		if( !cmfData.skeletons.empty() && ImGui::TreeNodeEx( &cmfData.skeletons, ImGuiTreeNodeFlags_DefaultOpen, "Skeletons (%zu)", cmfData.skeletons.size() ) )
		{
			for( const auto& skeleton : cmfData.skeletons )
			{
				renderSkeleton( skeleton );
			}
			ImGui::TreePop();
		}

		if( !cmfData.animations.empty() && ImGui::TreeNodeEx( &cmfData.animations, ImGuiTreeNodeFlags_DefaultOpen, "Animations (%zu)", cmfData.animations.size() ) )
		{
			for( const auto& anim : cmfData.animations )
			{
				clickableNode( SelectedItem::Animation, anim, ToStdString( anim.name ).c_str() );
			}
			ImGui::TreePop();
		}

		ImGui::TreePop();
	}
}

void UIDetailWindow::RenderVertexData( const CmfContent& cmfContent, const cmf::Span<cmf::VertexElement>& decl, const cmf::BufferView& vb )
{
	if( decl.empty() )
	{
		ImGui::Text( "No vertex declaration" );
		return;
	}
	if( vb.stride == 0 || vb.size == 0 )
	{
		ImGui::Text( "Empty vertex buffer" );
		return;
	}

	const uint32_t vertexCount = vb.size / vb.stride;
	ImGui::Text( "Vertices: %u   Stride: %u bytes", vertexCount, vb.stride );
	const uint8_t* vbData = cmfContent.Index( vb.index, 0 ) + vb.offset;

	auto isVisible = [&]( cmf::Usage usage, uint8_t usageIndex ) {
		return std::find( m_hiddenAttributes.begin(), m_hiddenAttributes.end(), std::make_pair( usage, usageIndex ) ) == m_hiddenAttributes.end();
	};

	// Vertex attribute filter list
	if( ImGui::CollapsingHeader( "Filters" ) )
	{
		auto unhideAttribute = [&]( cmf::Usage usage, uint8_t usageIndex ) {
			auto found = std::find( m_hiddenAttributes.begin(), m_hiddenAttributes.end(), std::make_pair( usage, usageIndex ) );
			if( found != m_hiddenAttributes.end() )
			{
				m_hiddenAttributes.erase( found );
			}
		};

		if( ImGui::Button( "Reset##vf" ) )
		{
			for( const auto& attr : decl )
			{
				unhideAttribute( attr.usage, attr.usageIndex );
			}
		}
		for( const auto& attr : decl )
		{
			bool enabled = isVisible( attr.usage, attr.usageIndex );
			if( ImGui::Checkbox( GetUsageFlagLabel( attr.usage, attr.usageIndex ).c_str(), &enabled ) )
			{
				if( enabled )
				{
					unhideAttribute( attr.usage, attr.usageIndex );
				}
				else
				{
					m_hiddenAttributes.emplace_back( attr.usage, attr.usageIndex );
				}
			}
		}
	}

	std::vector<cmf::VertexElement> filteredAttributes;
	for( const auto& attr : decl )
	{
		if( isVisible( attr.usage, attr.usageIndex ) )
		{
			filteredAttributes.push_back( attr );
		}
	}

	RenderAttributeTable( vbData, vertexCount, vb.stride, filteredAttributes );
}

void UIDetailWindow::RenderAttributeTable( const uint8_t* vbData, uint32_t vertexCount, uint32_t stride, const std::vector<cmf::VertexElement>& attributes )
{
	if( attributes.empty() )
	{
		ImGui::Text( "No readable vertex attributes" );
		return;
	}

	const ImGuiTableFlags tableFlags =
		ImGuiTableFlags_Borders |
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_ScrollX |
		ImGuiTableFlags_ScrollY |
		ImGuiTableFlags_SizingFixedFit;

	const int colCount = int( attributes.size() + 1 );
	const ImVec2 outerSize( 0.0f, ImGui::GetContentRegionAvail().y );
	if( ImGui::BeginTable( "##vertexdata", colCount, tableFlags, outerSize ) )
	{
		if( m_selectedItem.scrollTo && !m_selectedItem.selectedIndices.empty() )
		{
			const float targetY = (float)m_selectedItem.selectedIndices.front() * ImGui::GetTextLineHeightWithSpacing();
			ImGui::SetScrollY( targetY );
			m_selectedItem.scrollTo = false;
		}

		ImGui::TableSetupScrollFreeze( 1, 1 );
		ImGui::TableSetupColumn( "Index", ImGuiTableColumnFlags_WidthFixed, 48.0f );
		for( const auto& attr : attributes )
		{
			ImGui::TableSetupColumn( GetUsageFlagLabel( attr.usage, attr.usageIndex ).c_str(), ImGuiTableColumnFlags_WidthStretch );
		}
		ImGui::TableHeadersRow();

		std::vector<cmf::ConstBufferElementStream<Vector4>> attributeStreams;
		attributeStreams.reserve( attributes.size() );
		for( const auto& attr : attributes )
		{
			attributeStreams.emplace_back( attr, vbData, vertexCount, stride );
		}

		ImGuiListClipper clipper;
		clipper.Begin( int( vertexCount ) );
		while( clipper.Step() )
		{
			for( int vi = clipper.DisplayStart; vi < clipper.DisplayEnd; ++vi )
			{
				ImGui::TableNextRow();
				if( std::find( m_selectedItem.selectedIndices.begin(), m_selectedItem.selectedIndices.end(), uint32_t( vi ) ) != m_selectedItem.selectedIndices.end() )
				{
					ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32( ImGuiCol_Header ) );
				}
				ImGui::TableSetColumnIndex( 0 );
				ImGui::Text( "%d", vi );

				for( int ai = 0; ai < (int)attributes.size(); ++ai )
				{
					const auto& attr = attributes[ai];
					const auto& stream = attributeStreams[ai];

					ImGui::TableSetColumnIndex( ai + 1 );

					if( attr.usage == cmf::Usage::Color )
					{
						auto color = stream[vi];
						const float sz = ImGui::GetTextLineHeight();
						const ImVec2 p = ImGui::GetCursorScreenPos();
						ImGui::GetWindowDrawList()->AddRectFilled(
							p,
							ImVec2( p.x + sz, p.y + sz ),
							ImColor( color.x, color.y, color.z ) );
						ImGui::Dummy( ImVec2( sz + 4.0f, sz ) );
						ImGui::SameLine();
					}

					switch( attr.elementCount )
					{
					case 2:
						ImGui::Text( "%.4f  %.4f", stream[vi].x, stream[vi].y );
						break;
					case 3:
						ImGui::Text( "%.4f  %.4f  %.4f", stream[vi].x, stream[vi].y, stream[vi].z );
						break;
					case 4:
						ImGui::Text( "%.4f  %.4f  %.4f  %.4f", stream[vi].x, stream[vi].y, stream[vi].z, stream[vi].w );
						break;
					default:
						ImGui::Text( "%.4f", stream[vi].x );
						break;
					}
				}
			}
		}
		clipper.End();
		ImGui::EndTable();
	}
}

void UIDetailWindow::RenderIndexData( const CmfContent& cmfContent, const cmf::Mesh& mesh, const cmf::MeshLod& lod )
{
	const auto& ib = lod.ib;
	const auto& areas = mesh.areas;
	const auto& lodAreas = lod.areas;

	if( ib.stride == 0 || ib.size == 0 )
	{
		ImGui::Text( "Empty index buffer" );
		return;
	}

	const uint32_t indexCount = ib.size / ib.stride;
	const uint8_t* ibData = cmfContent.Index( ib.index, 0 ) + ib.offset;
	const cmf::IndexConverter indexConv( ib.stride );

	auto indexSelectable = [&]( uint32_t vertexIdx, int uniqueId ) {
		ImGui::PushID( uniqueId );
		const bool isSelected = std::find( m_selectedItem.selectedIndices.begin(), m_selectedItem.selectedIndices.end(), vertexIdx ) != m_selectedItem.selectedIndices.end();
		if( ImGui::Selectable( "", isSelected, ImGuiSelectableFlags_AllowOverlap ) )
		{
			m_selectedItem.selectedIndices = { vertexIdx };
		}
		ImGui::SameLine();
		if( ImGui::TextLink( std::to_string( vertexIdx ).c_str() ) )
		{
			m_selectedItem = SelectedItem{ SelectedItem::Type::VertexBuffer, &lod.vb, { vertexIdx }, true };
		}
		ImGui::PopID();
	};

	const uint32_t triangleCount = indexCount / 3;
	ImGui::Text( "Triangles: %u   Indices: %u", triangleCount, indexCount );

	const ImGuiTableFlags tableFlags =
		ImGuiTableFlags_Borders |
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_ScrollY |
		ImGuiTableFlags_SizingFixedFit;

	const ImVec2 outerSize( 0.0f, ImGui::GetContentRegionAvail().y );
	if( ImGui::BeginTable( "##indexdata", areas.empty() ? 4 : 5, tableFlags, outerSize ) )
	{
		ImGui::TableSetupScrollFreeze( 0, 1 );
		ImGui::TableSetupColumn( "Triangle", ImGuiTableColumnFlags_WidthFixed, 72.0f );
		ImGui::TableSetupColumn( "V0", ImGuiTableColumnFlags_WidthStretch );
		ImGui::TableSetupColumn( "V1", ImGuiTableColumnFlags_WidthStretch );
		ImGui::TableSetupColumn( "V2", ImGuiTableColumnFlags_WidthStretch );
		if( !areas.empty() )
		{
			ImGui::TableSetupColumn( "Area", ImGuiTableColumnFlags_WidthStretch );
		}
		ImGui::TableHeadersRow();

		ImGuiListClipper clipper;
		clipper.Begin( (int)triangleCount );
		while( clipper.Step() )
		{
			for( int ti = clipper.DisplayStart; ti < clipper.DisplayEnd; ++ti )
			{
				const uint8_t* base = ibData + (size_t)ti * 3 * ib.stride;
				const uint32_t v0 = indexConv( base );
				const uint32_t v1 = indexConv( base + ib.stride );
				const uint32_t v2 = indexConv( base + 2 * ib.stride );

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex( 0 );
				ImGui::Text( "%d", ti );
				ImGui::TableSetColumnIndex( 1 );
				indexSelectable( v0, ti * 3 );
				ImGui::TableSetColumnIndex( 2 );
				indexSelectable( v1, ti * 3 + 1 );
				ImGui::TableSetColumnIndex( 3 );
				indexSelectable( v2, ti * 3 + 2 );
				if( !areas.empty() )
				{
					ImGui::TableSetColumnIndex( 4 );
					for( const auto& area : lodAreas )
					{
						if( ti >= int( area.firstElement ) && ti < int( area.firstElement + area.elementCount ) )
						{
							auto areaIdx = std::distance( lodAreas.begin(), &area );
							ImGui::Text( "[%i] %s", int( areaIdx ), areas[areaIdx].name.empty() ? "Unnamed Area" : ToStdString( areas[areaIdx].name ).c_str() );
							break;
						}
					}
				}
			}
		}
		clipper.End();
		ImGui::EndTable();
	}
}

void UIDetailWindow::RenderSkeleton( const cmf::Skeleton& skeleton )
{
	ImGui::Text( "Skeleton: %s  Bones: %zu", cmf::ToStdString( skeleton.name ).c_str(), skeleton.bones.size() );

	if( ImGui::CollapsingHeader( "Filters" ) )
	{
		if( ImGui::Button( "Reset##bf" ) )
		{
			for( auto& [name, enabled] : m_boneColumnFilter )
			{
				enabled = true;
			}
		}
		for( auto& [name, enabled] : m_boneColumnFilter )
		{
			ImGui::Checkbox( name.c_str(), &enabled );
		}
	}

	std::vector<int> activeColIndices;
	for( size_t i = 0; i < m_boneColumnFilter.size(); ++i )
	{
		if( m_boneColumnFilter[i].second )
		{
			activeColIndices.push_back( int( i ) );
		}
	}

	const ImGuiTableFlags tableFlags =
		ImGuiTableFlags_Borders |
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_ScrollX |
		ImGuiTableFlags_ScrollY |
		ImGuiTableFlags_SizingFixedFit;

	const int colCount = int( activeColIndices.size() + 1 );
	if( ImGui::BeginTable( "##boneslist", colCount, tableFlags, ImVec2( 0.0f, ImGui::GetContentRegionAvail().y ) ) )
	{
		if( m_selectedItem.scrollTo && !m_selectedItem.selectedIndices.empty() )
		{
			ImGui::SetScrollY( (float)m_selectedItem.selectedIndices.front() * ImGui::GetTextLineHeightWithSpacing() );
			m_selectedItem.scrollTo = false;
		}

		ImGui::TableSetupScrollFreeze( 1, 1 );
		ImGui::TableSetupColumn( "Index", ImGuiTableColumnFlags_WidthFixed, 48.0f );
		for( const int ci : activeColIndices )
		{
			ImGui::TableSetupColumn( m_boneColumnFilter[ci].first.c_str(), ImGuiTableColumnFlags_WidthStretch );
		}
		ImGui::TableHeadersRow();

		ImGuiListClipper clipper;
		clipper.Begin( (int)skeleton.bones.size() );
		while( clipper.Step() )
		{
			for( int bi = clipper.DisplayStart; bi < clipper.DisplayEnd; ++bi )
			{
				ImGui::PushID( bi );

				ImGui::TableNextRow();

				for( int col = 0; col < int( activeColIndices.size() ); ++col )
				{
					ImGui::TableSetColumnIndex( col + 1 );
					switch( activeColIndices[col] )
					{
					case 0: // Name
						ImGui::TextUnformatted( cmf::ToStdString( skeleton.bones[bi] ).c_str() );
						break;
					case 1: // Parent
						if( skeleton.parents[bi] == uint32_t( bi ) || skeleton.parents[bi] >= uint32_t( skeleton.bones.size() ) )
						{
							ImGui::TextUnformatted( "-" );
						}
						else
						{
							ImGui::PushID( bi );
							if( ImGui::TextLink( cmf::ToStdString( skeleton.bones[skeleton.parents[bi]] ).c_str() ) )
							{
								m_selectedItem.selectedIndices = { skeleton.parents[bi] };
								m_selectedItem.scrollTo = true;
							}
							ImGui::PopID();
						}
						break;
					case 2: // Position
					{
						const auto& p = skeleton.restTransforms[bi].position;
						ImGui::Text( "%.4f  %.4f  %.4f", p.x, p.y, p.z );
					}
					break;
					case 3: // Rotation
					{
						const auto& r = skeleton.restTransforms[bi].rotation;
						ImGui::Text( "%.4f  %.4f  %.4f  %.4f", r.x, r.y, r.z, r.w );
					}
					break;
					case 4: // Scale
					{
						const auto& s = skeleton.restTransforms[bi].scale;
						ImGui::Text( "%.4f  %.4f  %.4f", s.x, s.y, s.z );
					}
					break;
					default:
						break;
					}
				}

				ImGui::TableSetColumnIndex( 0 );

				const auto foundSelected = std::find( m_selectedItem.selectedIndices.begin(), m_selectedItem.selectedIndices.end(), uint32_t( bi ) );
				if( ImGui::Selectable( std::to_string( bi ).c_str(), foundSelected != m_selectedItem.selectedIndices.end(), ImGuiSelectableFlags_SpanAllColumns ) )
				{
					if( ImGui::GetIO().KeyCtrl )
					{
						if( foundSelected != m_selectedItem.selectedIndices.end() )
						{
							m_selectedItem.selectedIndices.erase( foundSelected );
						}
						else
						{
							m_selectedItem.selectedIndices.push_back( uint32_t( bi ) );
						}
					}
					else
					{
						m_selectedItem.selectedIndices = { uint32_t( bi ) };
					}
				}

				ImGui::PopID();
			}
		}
		clipper.End();
		ImGui::EndTable();
	}
}

void UIDetailWindow::RenderBoneBindings( const CmfContent& cmfContent, const cmf::Mesh& mesh )
{
	const auto& skeletons = cmfContent.m_cmfData->skeletons;
	const bool hasSkeleton = mesh.skeleton != NO_SKELETON && size_t( mesh.skeleton ) < skeletons.size();
	const bool hasBoneBindings = !mesh.boneBindings.empty();

	if( !hasSkeleton && !hasBoneBindings )
	{
		ImGui::Text( "No bone data" );
		return;
	}

	if( hasBoneBindings )
	{
		ImGui::Text( "Bindings: %zu", mesh.boneBindings.size() );

		const ImGuiTableFlags tableFlags =
			ImGuiTableFlags_Borders |
			ImGuiTableFlags_RowBg |
			ImGuiTableFlags_ScrollY |
			ImGuiTableFlags_SizingFixedFit;

		if( ImGui::BeginTable( "##bonebindingstable", 2, tableFlags ) )
		{
			ImGui::TableSetupScrollFreeze( 0, 1 );
			ImGui::TableSetupColumn( "Index", ImGuiTableColumnFlags_WidthFixed, 48.0f );
			ImGui::TableSetupColumn( "Name", ImGuiTableColumnFlags_WidthStretch );
			ImGui::TableHeadersRow();

			ImGuiListClipper clipper;
			clipper.Begin( (int)mesh.boneBindings.size() );
			while( clipper.Step() )
			{
				const cmf::Skeleton* skeleton = nullptr;
				if( mesh.skeleton != NO_SKELETON )
				{
					skeleton = &cmfContent.m_cmfData->skeletons[mesh.skeleton];
				}
				for( int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i )
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex( 0 );
					ImGui::Text( "%d", i );
					ImGui::TableSetColumnIndex( 1 );

					if( skeleton )
					{
						if( ImGui::TextLink( cmf::ToStdString( mesh.boneBindings[i].name ).c_str() ) )
						{
							const auto* found = std::find( skeleton->bones.begin(), skeleton->bones.end(), mesh.boneBindings[i].name );
							if( found != skeleton->bones.end() )
							{
								const auto boneIndex = static_cast<uint32_t>( std::distance( skeleton->bones.begin(), found ) );
								m_selectedItem = SelectedItem{ SelectedItem::Type::SkeletonBones, skeleton, { boneIndex }, true };
							}
							else
							{
								m_selectedItem = SelectedItem{ SelectedItem::Type::SkeletonBones, skeleton };
							}
						}
					}
					else
					{
						ImGui::TextUnformatted( cmf::ToStdString( mesh.boneBindings[i].name ).c_str() );
					}
				}
			}
			clipper.End();
			ImGui::EndTable();
		}
	}
}

void UIDetailWindow::RenderAnimationChannels( const cmf::Animation& anim, const cmf::Data& data )
{
	ImGui::Text( "Name: %s", ToStdString( anim.name ).c_str() );
	ImGui::Text( "Duration: %.4f s", anim.duration );
	ImGui::Text( "Channels: %zu   Curves: %zu", anim.channels.size(), anim.curves.size() );

	ImGui::Separator();

	if( anim.channels.empty() )
	{
		ImGui::Text( "No channels" );
		return;
	}

	const ImGuiTableFlags tableFlags =
		ImGuiTableFlags_Borders |
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_ScrollX |
		ImGuiTableFlags_ScrollY |
		ImGuiTableFlags_SizingFixedFit;

	if( ImGui::BeginTable( "##channelstable", 3, tableFlags, ImVec2( 0.0f, ImGui::GetContentRegionAvail().y ) ) )
	{
		ImGui::TableSetupScrollFreeze( 0, 1 );
		ImGui::TableSetupColumn( "Target", ImGuiTableColumnFlags_WidthStretch );
		ImGui::TableSetupColumn( "Type", ImGuiTableColumnFlags_WidthFixed );
		ImGui::TableSetupColumn( "Curve", ImGuiTableColumnFlags_WidthFixed );

		ImGui::TableHeadersRow();

		ImGuiListClipper clipper;
		clipper.Begin( int( anim.channels.size() ) );
		while( clipper.Step() )
		{
			for( int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i )
			{
				const auto& channel = anim.channels[i];

				ImGui::TableNextRow();
				if( std::find( m_selectedItem.selectedIndices.begin(), m_selectedItem.selectedIndices.end(), uint32_t( i ) ) != m_selectedItem.selectedIndices.end() )
				{
					ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32( ImGuiCol_Header ) );
				}

				ImGui::TableSetColumnIndex( 0 );
				ImGui::PushID( i );
				bool foundTarget = false;
				switch( channel.targetType )
				{
				case cmf::AnimationChannelTargetType::BonePosition:
				case cmf::AnimationChannelTargetType::BoneRotation:
				case cmf::AnimationChannelTargetType::BoneScale:
					for( const auto& skeleton : data.skeletons )
					{
						const auto* bone = std::find( skeleton.bones.begin(), skeleton.bones.end(), channel.target );
						if( bone != skeleton.bones.end() )
						{
							if( ImGui::TextLink( cmf::ToStdString( *bone ).c_str() ) )
							{
								m_selectedItem = SelectedItem{ SelectedItem::Type::SkeletonBones, &skeleton, { uint32_t( std::distance( skeleton.bones.begin(), bone ) ) }, true };
							}
							foundTarget = true;
							break;
						}
					}
					break;
				case cmf::AnimationChannelTargetType::MorphTarget:
					for( const auto& mesh : data.meshes )
					{
						for( const auto& target : mesh.morphTargets.targets )
						{
							if( target.name == channel.target )
							{
								if( ImGui::TextLink( cmf::ToStdString( target.name ).c_str() ) )
								{
									m_selectedItem = SelectedItem{ SelectedItem::Type::VertexBuffer, &mesh.lods[0].morphTargets[std::distance( mesh.morphTargets.targets.begin(), &target )].vb };
								}
								foundTarget = true;
								break;
							}
						}
						if( foundTarget )
						{
							break;
						}
					}
					break;
				default:
					break;
				}
				if( !foundTarget )
				{
					ImGui::TextUnformatted( cmf::ToStdString( channel.target ).c_str() );
				}
				ImGui::PopID();

				ImGui::TableSetColumnIndex( 1 );
				ImGui::TextUnformatted( GetAnimationTargetTypeName( channel.targetType ) );

				ImGui::TableSetColumnIndex( 2 );
				ImGui::PushID( i );
				if( ImGui::TextLink( std::to_string( channel.curveIndex ).c_str() ) )
				{
					m_selectedItem = SelectedItem{ SelectedItem::Type::AnimationCurve, &anim.curves[channel.curveIndex] };
				}
				ImGui::PopID();
			}
		}
		clipper.End();
		ImGui::EndTable();
	}
}

void UIDetailWindow::RenderAnimationCurves( const cmf::AnimationCurve& curve, const cmf::Animation& anim )
{
	const auto curveIndex = uint32_t( std::distance( anim.curves.begin(), &curve ) );
	ImGui::Text( "Index: %u", curveIndex );
	ImGui::Text( "Knots: %u", curve.knotCount );
	ImGui::Text( "Knot Type: %s", GetElementTypeName( curve.knotType ) );
	ImGui::Text( "Value Type: %s%s",
				 GetElementTypeName( curve.valueType ),
				 curve.valueDimension > 1 ? ( " x" + std::to_string( curve.valueDimension ) ).c_str() : "" );
	ImGui::Text( "Interpolation: %s", curve.interpolation == cmf::Interpolation::Linear ? "Linear" : "Step" );

	const uint32_t targetCount = std::accumulate( anim.channels.begin(), anim.channels.end(), 0u, [&]( uint32_t sum, const cmf::AnimationChannel& channel ) {
		return sum + ( channel.curveIndex == curveIndex ? 1 : 0 );
	} );
	ImGui::Text( "Targets: " );
	ImGui::SameLine();
	if( targetCount )
	{
		if( ImGui::TextLink( std::to_string( targetCount ).c_str() ) )
		{
			std::vector<uint32_t> linkedChannels;
			for( uint32_t ci = 0; ci < (uint32_t)anim.channels.size(); ++ci )
			{
				if( anim.channels[ci].curveIndex == curveIndex )
				{
					linkedChannels.push_back( ci );
				}
			}
			m_selectedItem = SelectedItem{ SelectedItem::Type::Animation, &anim, linkedChannels };
		}
	}
	else
	{
		ImGui::Text( "%u", targetCount );
	}

	const ImGuiTableFlags tableFlags =
		ImGuiTableFlags_Borders |
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_ScrollX |
		ImGuiTableFlags_ScrollY |
		ImGuiTableFlags_SizingFixedFit;

	if( ImGui::BeginTable( "##curvestable", 3, tableFlags, ImVec2( 0.0f, ImGui::GetContentRegionAvail().y ) ) )
	{
		ImGui::TableSetupScrollFreeze( 1, 1 );
		ImGui::TableSetupColumn( "Knot", ImGuiTableColumnFlags_WidthFixed, 48.0f );
		ImGui::TableSetupColumn( "Time", ImGuiTableColumnFlags_WidthFixed, 80.0f );
		ImGui::TableSetupColumn( "Value", ImGuiTableColumnFlags_WidthFixed );
		ImGui::TableHeadersRow();

		cmf::VertexElement element = {};
		element.type = curve.knotType;
		element.elementCount = 1;
		const auto stride = cmf::GetVertexElementSize( element );
		const cmf::ConstBufferElementStream<float> knots{ element, curve.knots.data(), uint32_t( curve.knots.size() / stride ), stride };

		element.type = curve.valueType;
		const auto valueStride = cmf::GetVertexElementSize( element );
		const cmf::ConstBufferElementStream<float> values{ element, curve.values.data(), uint32_t( curve.values.size() / valueStride ), valueStride };

		ImGuiListClipper clipper;
		clipper.Begin( int( curve.knotCount ) );
		while( clipper.Step() )
		{
			for( int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i )
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex( 0 );
				ImGui::Text( "%d", i );
				ImGui::TableSetColumnIndex( 1 );
				ImGui::Text( "%.2f", knots[i] );
				ImGui::TableSetColumnIndex( 2 );
				switch( curve.valueDimension )
				{
				case 1:
					ImGui::Text( "%.4f", values[i * curve.valueDimension] );
					break;
				case 2:
					ImGui::Text( "%.4f  %.4f", values[i * curve.valueDimension], values[i * curve.valueDimension + 1] );
					break;
				case 3:
					ImGui::Text( "%.4f  %.4f  %.4f", values[i * curve.valueDimension], values[i * curve.valueDimension + 1], values[i * curve.valueDimension + 2] );
					break;
				case 4:
					ImGui::Text( "%.4f  %.4f  %.4f  %.4f", values[i * curve.valueDimension], values[i * curve.valueDimension + 1], values[i * curve.valueDimension + 2], values[i * curve.valueDimension + 3] );
					break;
				default:
					break;
				}
			}
		}
		clipper.End();
		ImGui::EndTable();
	}
}

void UIDetailWindow::RenderAudioOccluder( const cmf::AudioOcclusionMesh& audioOcclusionMesh )
{
	if( audioOcclusionMesh.vertices.empty() && audioOcclusionMesh.indices.empty() )
	{
		ImGui::Text( "No audio occlusion mesh" );
		return;
	}

	const auto vertexCount = uint32_t( audioOcclusionMesh.vertices.size() );
	const auto triCount = uint32_t( audioOcclusionMesh.indices.size() / 3 );

	ImGui::Text( "Vertices: %u  Triangles: %u", vertexCount, triCount );
	const auto& b = audioOcclusionMesh.bounds;
	auto sz = b.Size();
	ImGui::Text( "Bounds Min:  %.4f  %.4f  %.4f", b.m_min.x, b.m_min.y, b.m_min.z );
	ImGui::Text( "Bounds Max:  %.4f  %.4f  %.4f", b.m_max.x, b.m_max.y, b.m_max.z );
	ImGui::Text( "Bounds Size: %.4f  %.4f  %.4f", sz.x, sz.y, sz.z );

	ImGui::Spacing();

	const float indexColumnWidth = 60.0f;
	const float columnWidth = 80.0f;

	if( ImGui::CollapsingHeader( "Vertices", ImGuiTreeNodeFlags_DefaultOpen ) )
	{
		const ImGuiTableFlags tableFlags =
			ImGuiTableFlags_Borders |
			ImGuiTableFlags_RowBg |
			ImGuiTableFlags_ScrollX |
			ImGuiTableFlags_ScrollY |
			ImGuiTableFlags_SizingFixedFit;

		const float tableHeight = std::min( (float)vertexCount * ImGui::GetTextLineHeightWithSpacing() + ImGui::GetTextLineHeightWithSpacing(), 200.0f );
		if( ImGui::BeginTable( "##aomverts", 4, tableFlags, ImVec2( 0.0f, tableHeight ) ) )
		{
			ImGui::TableSetupScrollFreeze( 1, 1 );
			ImGui::TableSetupColumn( "Index", ImGuiTableColumnFlags_WidthFixed, indexColumnWidth );
			ImGui::TableSetupColumn( "X", ImGuiTableColumnFlags_WidthFixed, columnWidth );
			ImGui::TableSetupColumn( "Y", ImGuiTableColumnFlags_WidthFixed, columnWidth );
			ImGui::TableSetupColumn( "Z", ImGuiTableColumnFlags_WidthStretch );
			ImGui::TableHeadersRow();

			ImGuiListClipper clipper;
			clipper.Begin( (int)vertexCount );
			while( clipper.Step() )
			{
				for( int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i )
				{
					const auto& v = audioOcclusionMesh.vertices[i];
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex( 0 );
					ImGui::Text( "%d", i );
					ImGui::TableSetColumnIndex( 1 );
					ImGui::Text( "%.4f", v.x );
					ImGui::TableSetColumnIndex( 2 );
					ImGui::Text( "%.4f", v.y );
					ImGui::TableSetColumnIndex( 3 );
					ImGui::Text( "%.4f", v.z );
				}
			}
			clipper.End();
			ImGui::EndTable();
		}
	}

	ImGui::Spacing();

	if( triCount > 0 && ImGui::CollapsingHeader( "Triangles", ImGuiTreeNodeFlags_DefaultOpen ) )
	{
		const ImGuiTableFlags tableFlags =
			ImGuiTableFlags_Borders |
			ImGuiTableFlags_RowBg |
			ImGuiTableFlags_ScrollX |
			ImGuiTableFlags_ScrollY |
			ImGuiTableFlags_SizingFixedFit;

		const float tableHeight = std::min( (float)triCount * ImGui::GetTextLineHeightWithSpacing() + ImGui::GetTextLineHeightWithSpacing(), 200.0f );
		if( ImGui::BeginTable( "##aomtris", 4, tableFlags, ImVec2( 0.0f, tableHeight ) ) )
		{
			ImGui::TableSetupScrollFreeze( 1, 1 );
			ImGui::TableSetupColumn( "Triangle", ImGuiTableColumnFlags_WidthFixed, indexColumnWidth );
			ImGui::TableSetupColumn( "V0", ImGuiTableColumnFlags_WidthFixed, columnWidth );
			ImGui::TableSetupColumn( "V1", ImGuiTableColumnFlags_WidthFixed, columnWidth );
			ImGui::TableSetupColumn( "V2", ImGuiTableColumnFlags_WidthStretch );
			ImGui::TableHeadersRow();

			ImGuiListClipper clipper;
			clipper.Begin( (int)triCount );
			while( clipper.Step() )
			{
				for( int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i )
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex( 0 );
					ImGui::Text( "%d", i );
					ImGui::TableSetColumnIndex( 1 );
					ImGui::Text( "%u", (uint32_t)audioOcclusionMesh.indices[i * 3 + 0] );
					ImGui::TableSetColumnIndex( 2 );
					ImGui::Text( "%u", (uint32_t)audioOcclusionMesh.indices[i * 3 + 1] );
					ImGui::TableSetColumnIndex( 3 );
					ImGui::Text( "%u", (uint32_t)audioOcclusionMesh.indices[i * 3 + 2] );
				}
			}
			clipper.End();
			ImGui::EndTable();
		}
	}
}

// NOLINTEND(cppcoreguidelines-pro-type-vararg)
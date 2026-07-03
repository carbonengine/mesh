// Copyright © 2026 CCP ehf.

#include "uiGeneralWindow.h"

#include <algorithm>
#include <cmf/converters.h>
#include <cmf/bufferstreams.h>
#include <filesystem>
#include <iterator>
#include <imgui.h>

#include "uiCustomWidgets.h"

// ImGui is using a lot of variadic functions for text formatting, so we disable the cppcoreguidelines-pro-type-vararg lint for this file
// NOLINTBEGIN(cppcoreguidelines-pro-type-vararg)

namespace
{
using AxisTriCheckboxStates = std::vector<std::pair<uint32_t, ImGui::CheckBoxTriStateValue>>;
using AxisCheckboxStates = std::vector<std::pair<uint32_t, bool>>;

/***
* @brief Combines the states of a mesh state for a given axis into a single vector of checkbox states.
* @param meshState The mesh state to combine the axis states from.
* @param getAxisStates A callable that takes a MeshState and returns a StateCollection of pairs of uint32_t and bool representing the axis states.
*/
template <typename GetAxisStates>
AxisCheckboxStates GetAxisCheckboxStates( const MeshState& meshState, GetAxisStates getAxisStates )
{
	AxisCheckboxStates combinedStates;
	for( const auto& axis : getAxisStates( meshState ) )
	{
		auto [index, checked] = axis.GetValue();
		combinedStates.push_back( { index, checked } );
	}
	return combinedStates;
}

/***
* @brief Combines the states of multiple mesh states for a given axis into a single vector of tri-state checkbox states.
* @param meshStates The collection of mesh states to combine the axis states from.
* @param getAxisStates A callable that takes a MeshState and returns a StateCollection of pairs of uint32_t and bool representing the axis states.
*/
template <typename GetAxisStates>
AxisTriCheckboxStates GetAxisTriCheckboxStates( StateCollection<MeshState>& meshStates, GetAxisStates getAxisStates )
{
	AxisTriCheckboxStates combinedStates;

	for( auto& meshState : meshStates )
	{
		auto axisStatesForMesh = GetAxisCheckboxStates( meshState.GetValue(), getAxisStates );
		// combine the states
		for( const auto& state : axisStatesForMesh )
		{
			auto [index, checked] = state;

			auto foundState = std::find_if( combinedStates.begin(), combinedStates.end(), [&]( const auto& existingState ) {
				return existingState.first == index;
			} );
			auto stateAsTriState = checked ? ImGui::CheckBoxTriStateValue::CHECKED : ImGui::CheckBoxTriStateValue::UNCHECKED;
			if( foundState == combinedStates.end() )
			{
				combinedStates.push_back( { index, stateAsTriState } );
			}
			else if( foundState->second != stateAsTriState )
			{
				foundState->second = ImGui::CheckBoxTriStateValue::MIXED;
			}
		}
	}

	return combinedStates;
}

/***
* @brief Sets up a row of tri-state checkboxes for a model axis.
* @param checkboxStates The states of the checkboxes for the axis.
* @param name The name of the axis.
* @param changeCallback A callable that is invoked when a checkbox state changes.
*/
template <typename Callable>
void SetupModelAxisRow( AxisTriCheckboxStates& checkboxStates, const char* name, const Callable& changeCallback )
{
	const int32_t maxIndex = checkboxStates.empty() ? -1 : static_cast<int32_t>( std::max_element( checkboxStates.begin(), checkboxStates.end(), []( const auto& a, const auto& b ) {
																					 return a.first < b.first;
																				 } )->first );

	ImGui::BeginDisabled( maxIndex < 0 );
	ImGui::TableNextColumn();
	ImGui::Text( "%s", name );
	ImGui::SetItemTooltip( "Toggles the %s visualization for all meshes", name );
	ImGui::TableNextColumn();

	if( maxIndex >= 0 )
	{
		for( int32_t i = 0; i <= maxIndex; ++i )
		{
			auto foundState = std::find_if( checkboxStates.begin(), checkboxStates.end(), [i]( const auto& state ) {
				return state.first == static_cast<uint32_t>( i );
			} );

			if( foundState == checkboxStates.end() )
			{
				ImGui::BeginDisabled( true );
				auto disabledValue = ImGui::CheckBoxTriStateValue::UNCHECKED;
				ImGui::CheckBoxTristate( ( std::string( "##tricheckbox" ) + name + std::to_string( i ) ).c_str(), &disabledValue );
				ImGui::SetItemTooltip( "No mesh has %s information for usage %d", name, i );
				ImGui::SameLine();
				ImGui::EndDisabled();
			}
			else
			{
				auto value = foundState->second;
				if( ImGui::CheckBoxTristate( ( std::string( "##tricheckbox" ) + name + std::to_string( i ) ).c_str(), &value ) )
				{
					changeCallback( value == ImGui::CheckBoxTriStateValue::CHECKED, static_cast<uint32_t>( i ) );
				}
				ImGui::SetItemTooltip( "Toggles debug visualization for %s with usage index %d for all meshes that have it", name, i );
				ImGui::SameLine();
			}
		}
	}
	else
	{
		ImGui::BeginDisabled( true );
		bool value = false;
		ImGui::Checkbox( ( std::string( "##" ) + name ).c_str(), &value );
		ImGui::SetItemTooltip( "Model doesn't have any %s", name.c_str() );
		ImGui::SameLine();
		ImGui::EndDisabled();
	}
	ImGui::EndDisabled();
}

/***
* @brief Sets up a row of checkboxes for a mesh axis.
* @param checkboxStates The states of the checkboxes for the axis.
* @param name The name of the axis.
* @param changeCallback A callable that is invoked when a checkbox state changes.
*/
template <typename Callable>
void SetupMeshAxisRow( AxisCheckboxStates& checkboxStates, const char* name, const Callable& changeCallback )
{
	const int32_t maxIndex = checkboxStates.empty() ? -1 : static_cast<int32_t>( std::max_element( checkboxStates.begin(), checkboxStates.end(), []( const auto& a, const auto& b ) {
																					 return a.first < b.first;
																				 } )->first );

	ImGui::BeginDisabled( maxIndex < 0 );
	ImGui::TableNextColumn();
	ImGui::Text( "%s", name );
	ImGui::SetItemTooltip( "Toggles the %s visualization for this mesh", name );
	ImGui::TableNextColumn();

	if( maxIndex >= 0 )
	{
		for( int32_t i = 0; i <= maxIndex; ++i )
		{
			auto foundState = std::find_if( checkboxStates.begin(), checkboxStates.end(), [i]( const auto& state ) {
				return state.first == static_cast<uint32_t>( i );
			} );

			if( foundState == checkboxStates.end() )
			{
				ImGui::BeginDisabled( true );
				auto disabledValue = false;
				ImGui::Checkbox( ( std::string( "##tricheckbox" ) + name + std::to_string( i ) ).c_str(), &disabledValue );
				ImGui::SetItemTooltip( "No mesh has %s information for usage %d", name, i );
				ImGui::SameLine();
				ImGui::EndDisabled();
			}
			else
			{
				auto value = foundState->second;
				if( ImGui::Checkbox( ( std::string( "##tricheckbox" ) + name + std::to_string( i ) ).c_str(), &value ) )
				{
					changeCallback( value, static_cast<uint32_t>( i ) );
				}
				ImGui::SetItemTooltip( "Toggles debug visualization for %s with usage index %d for this mesh", name, i );
				ImGui::SameLine();
			}
		}
	}
	else
	{
		ImGui::BeginDisabled( true );
		bool value = false;
		ImGui::Checkbox( ( std::string( "##" ) + name ).c_str(), &value );
		ImGui::SetItemTooltip( "Mesh doesn't have any %s", name );
		ImGui::SameLine();
		ImGui::EndDisabled();
	}
	ImGui::EndDisabled();
}

template <typename GetAxisStates>
void SetAxisStateForMeshes( StateCollection<MeshState>& meshStates, uint32_t index, bool checked, GetAxisStates getAxisStates )
{
	for( auto& meshState : meshStates )
	{
		auto& axisStates = getAxisStates( meshState.GetValue() );
		auto foundState = std::find_if( axisStates.begin(), axisStates.end(), [index]( const State<std::pair<uint32_t, bool>>& state ) {
			return state.GetValue().first == index;
		} );
		if( foundState != axisStates.end() )
		{
			foundState->SetValue( { index, checked } );
		}
	}
}
}

void UIGeneralWindow::Render( AppState& appState, float marginTop, float marginBottom )
{
	const float width = (float)appState.windowSize.GetValue().first;
	const float height = (float)appState.windowSize.GetValue().second;

	const float ySize = std::max( 1.0f, height - marginTop - marginBottom + 1 ); // +1 so we get an overlap of the borders

	ImGui::SetNextWindowPos( ImVec2( 0, marginTop ), ImGuiCond_Always );
	ImGui::SetNextWindowSizeConstraints( ImVec2( width / 16.0f, ySize ), ImVec2( width, ySize ) );
	ImGui::SetNextWindowSize( ImVec2( width / 4.0f, ySize ), ImGuiCond_FirstUseEver );
	ImGui::Begin( "CMF Info", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings );
	RenderGeneralInfo( appState );
	RenderMeshList( appState );
	RenderSkeletonList( appState );
	RenderAnimationOverrideList( appState );
	ImGui::End();
}

void UIGeneralWindow::RenderGeneralInfo( AppState& appState )
{
	auto data = appState.cmfContent.GetValue();

	// fetch the data
	uint32_t vertexCount = 0;
	uint32_t indexCount = 0;
	size_t maxLod = 0;
	std::vector<std::string> lodNames{ "Auto" };
	if( data )
	{
		const auto& cmfMeshes = data->m_cmfData->meshes;
		const auto& modelStateMeshes = appState.modelState.meshes;
		assert( cmfMeshes.size() == modelStateMeshes.size() );

		for( uint32_t meshIndex = 0; meshIndex < cmfMeshes.size(); ++meshIndex )
		{
			const auto& mesh = cmfMeshes[meshIndex];

			auto& meshState = modelStateMeshes[meshIndex].GetValue();
			const auto& lod = mesh.lods[meshState.activeLod.GetValue()];

			vertexCount += cmf::GetStreamElementCount( lod.vb );
			indexCount += cmf::GetStreamElementCount( lod.ib );
			maxLod = std::max( maxLod, mesh.lods.size() );
		}

		for( size_t i = 0; i < maxLod; ++i )
		{
			lodNames.push_back( "Lod " + std::to_string( i ) );
		}
	}

	ImGui::SeparatorText( "General" );
	if( ImGui::BeginTable( "##table", 2 ) )
	{
		ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_WidthFixed );
		ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_WidthStretch );

		auto fileName = std::filesystem::path( appState.cmfPath.GetValue() ).filename().string();
		auto tooltip = std::string( "Full path: " ) + appState.cmfPath.GetValue();
		SetupAttribute( "Path", tooltip.c_str(), false, [&]() { ImGui::InputText( "##label", fileName.data(), fileName.size(), ImGuiInputTextFlags_ReadOnly ); } );

		SetupAttribute( "Lods", "Number of LODs in the model", data == nullptr, [&]() {
			ImGui::ComboBox( "##lod", lodNames, appState.modelState.selectedLod.GetValue() + 1, [&appState]( int32_t selectedLod ) {
				appState.modelState.selectedLod.SetValue( selectedLod - 1 );
			} );
		} );
		SetupAttribute( "Vertices", "Vertex count of all meshes", false, [&]() { ImGui::Text( "%u", vertexCount ); } );
		SetupAttribute( "Indices", "Index count of all meshes", false, [&]() { ImGui::Text( "%u", indexCount ); } );
		SetupAttribute( "Meshes", "Number of meshes", false, [&]() { ImGui::Text( "%u", data ? data->m_cmfData->meshes.size() : 0 ); } );

		SetupAttribute( "Visualization", "Visualization mode", data == nullptr, [&]() {
			const auto& availableShaders = appState.modelState.availableShaders.GetValue();

			// get the names
			std::vector<std::string> shaderNames;
			shaderNames.reserve( availableShaders.size() );
			std::transform( availableShaders.begin(), availableShaders.end(), std::back_inserter( shaderNames ), []( const auto& pair ) { return pair.first; } );

			// find the index of the active shader in the available shaders list
			const auto& activeShader = appState.modelState.activeShader.GetValue();
			const auto& foundActiveShader = std::find_if( availableShaders.begin(), availableShaders.end(), [&activeShader]( const auto& pair ) {
				return pair.first == activeShader.first && pair.second == activeShader.second;
			} );

			int32_t activeShaderIndex = static_cast<int32_t>( std::distance( availableShaders.begin(), foundActiveShader ) );

			ImGui::ComboBox( "##visualization", shaderNames, activeShaderIndex, [&appState]( int32_t selectedIndex ) {
				const auto& availableShaders = appState.modelState.availableShaders.GetValue();
				if( selectedIndex >= 0 && selectedIndex < static_cast<int32_t>( availableShaders.size() ) )
				{
					appState.modelState.activeShader.SetValue( availableShaders[selectedIndex] );
				}
			} );
		} );

		SetupAttribute( "Display", "Toggle display of all meshes", data == nullptr, [&]() {
			auto checkedCount = std::count_if( appState.modelState.meshes.begin(), appState.modelState.meshes.end(), []( const State<MeshState>& state ) {
				return state.GetValue().display.GetValue();
			} );
			ImGui::CheckBoxTriStateValue checkedState = ImGui::GetCheckedStatus( checkedCount, static_cast<uint32_t>( appState.modelState.meshes.size() ) );
			if( ImGui::CheckBoxTristate( "##displaycheckbox", &checkedState ) )
			{
				std::for_each( appState.modelState.meshes.begin(), appState.modelState.meshes.end(), [checkedState]( State<MeshState>& state ) {
					state.GetValue().display.SetValue( checkedState == ImGui::CheckBoxTriStateValue::CHECKED );
				} );
			};
		} );

		SetupAttribute( "Bounding Box", "Whole model bounding box", data == nullptr, [&] {
			bool checked = appState.modelState.modelBoundingBox.GetValue();
			if( ImGui::Checkbox( "##boundingboxcheckbox", &checked ) )
			{
				appState.modelState.modelBoundingBox.SetValue( checked );
			}
		} );

		SetupAttribute( "Wireframe Overlay", "Wireframe overlay for all meshes", data == nullptr, [&] {
			auto checkedCount = std::count_if( appState.modelState.meshes.begin(), appState.modelState.meshes.end(), []( const State<MeshState>& state ) {
				return state.GetValue().wireframeOverlay.GetValue();
			} );
			ImGui::CheckBoxTriStateValue checkedState = ImGui::GetCheckedStatus( checkedCount, static_cast<uint32_t>( appState.modelState.meshes.size() ) );
			if( ImGui::CheckBoxTristate( "##wireframecheckbox", &checkedState ) )
			{
				std::for_each( appState.modelState.meshes.begin(), appState.modelState.meshes.end(), [checkedState]( State<MeshState>& state ) {
					state.GetValue().wireframeOverlay.SetValue( checkedState == ImGui::CheckBoxTriStateValue::CHECKED );
				} );
			}
		} );

		bool hasAudioOcclusionMeshes = false;
		if( data )
		{
			hasAudioOcclusionMeshes = std::any_of( data->m_cmfData->meshes.begin(), data->m_cmfData->meshes.end(), []( const cmf::Mesh& mesh ) {
				return !mesh.audioOcclusionMesh.vertices.empty();
			} );
		}

		SetupAttribute( "Audio Occlusion Mesh", "Audio occlusion mesh for all meshes", !hasAudioOcclusionMeshes, [&]() {
			size_t checkedCount = std::count_if( appState.modelState.meshes.begin(), appState.modelState.meshes.end(), []( const State<MeshState>& state ) {
				return state.GetValue().audioOcclusionMesh.GetValue();
			} );
			ImGui::CheckBoxTriStateValue checkedState = ImGui::GetCheckedStatus( checkedCount, static_cast<uint32_t>( appState.modelState.meshes.size() ) );
			if( ImGui::CheckBoxTristate( "##audioocclusioncheckbox", &checkedState ) )
			{
				std::for_each( appState.modelState.meshes.begin(), appState.modelState.meshes.end(), [checkedState]( State<MeshState>& state ) {
					state.GetValue().audioOcclusionMesh.SetValue( checkedState == ImGui::CheckBoxTriStateValue::CHECKED );
				} );
			}
		} );

		auto normalCheckboxes = GetAxisTriCheckboxStates( appState.modelState.meshes, []( const MeshState& meshState ) -> auto& {
			return meshState.showVertexNormals;
		} );
		auto tangentCheckboxes = GetAxisTriCheckboxStates( appState.modelState.meshes, []( const MeshState& meshState ) -> auto& {
			return meshState.showVertexTangents;
		} );
		auto binormalCheckboxes = GetAxisTriCheckboxStates( appState.modelState.meshes, []( const MeshState& meshState ) -> auto& {
			return meshState.showVertexBinormals;
		} );

		SetupModelAxisRow( normalCheckboxes, "Normals", [&appState]( bool checked, uint32_t index ) {
			SetAxisStateForMeshes( appState.modelState.meshes, index, checked, []( MeshState& meshState ) -> auto& {
				return meshState.showVertexNormals;
			} );
		} );

		SetupModelAxisRow( tangentCheckboxes, "Tangents", [&appState]( bool checked, uint32_t index ) {
			SetAxisStateForMeshes( appState.modelState.meshes, index, checked, []( MeshState& meshState ) -> auto& {
				return meshState.showVertexTangents;
			} );
		} );

		SetupModelAxisRow( binormalCheckboxes, "Binormals", [&appState]( bool checked, uint32_t index ) {
			SetAxisStateForMeshes( appState.modelState.meshes, index, checked, []( MeshState& meshState ) -> auto& {
				return meshState.showVertexBinormals;
			} );
		} );

		ImGui::EndTable();
	}
}

void UIGeneralWindow::RenderMeshList( AppState& appState )
{
	std::string header = "Meshes (" + std::to_string( appState.modelState.meshes.size() ) + ")";

	ImGui::SeparatorText( header.c_str() );
	const auto data = appState.cmfContent.GetValue();
	size_t meshCount = data != nullptr && data->m_cmfData != nullptr ? data->m_cmfData->meshes.size() : 0;

	if( meshCount != appState.modelState.meshes.size() )
	{
		return;
	}

	for( int32_t index = 0; index < appState.modelState.meshes.size(); ++index )
	{
		const auto& mesh = data->m_cmfData->meshes[index];
		auto& meshState = appState.modelState.meshes[index].GetValue();

		RenderMeshInfo( mesh, meshState );
	}
}

void UIGeneralWindow::RenderMeshInfo( const cmf::Mesh& mesh, MeshState& meshState )
{
	if( ImGui::TreeNode( cmf::ToStdString( mesh.name ).c_str() ) )
	{
		if( ImGui::BeginTable( "##table", 2 ) )
		{
			ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_WidthFixed );
			ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_WidthStretch );
			ImGui::TableNextRow();
			SetupAttribute( "Display", "Toggle display of this mesh", false, [&]() {
				bool checked = meshState.display.GetValue();
				if( ImGui::Checkbox( "##displaycheckbox", &checked ) )
				{
					meshState.display.SetValue( checked );
				}
			} );
			SetupAttribute( "Bounding Box", "Toggles the bounding box of this mesh", false, [&]() {
				bool checked = meshState.renderBoundingBox.GetValue();
				if( ImGui::Checkbox( "##boundingboxcheckbox", &checked ) )
				{
					meshState.renderBoundingBox.SetValue( checked );
				}
			} );
			SetupAttribute( "Wireframe Overlay", "Wireframe overlay for this mesh", false, [&]() {
				bool checked = meshState.wireframeOverlay.GetValue();
				if( ImGui::Checkbox( "##wireframecheckbox", &checked ) )
				{
					meshState.wireframeOverlay.SetValue( checked );
				}
			} );

			SetupAttribute( "Audio Occlusion Mesh", "Audio occlusion mesh for this mesh", mesh.audioOcclusionMesh.vertices.empty(), [&]() {
				bool checked = meshState.audioOcclusionMesh.GetValue();
				if( ImGui::Checkbox( "##audioocclusioncheckbox", &checked ) )
				{
					meshState.audioOcclusionMesh.SetValue( checked );
				}
			} );

			auto normalCheckboxes = GetAxisCheckboxStates( meshState, []( const MeshState& meshState ) -> auto& {
				return meshState.showVertexNormals;
			} );
			auto tangentCheckboxes = GetAxisCheckboxStates( meshState, []( const MeshState& meshState ) -> auto& {
				return meshState.showVertexTangents;
			} );
			auto binormalCheckboxes = GetAxisCheckboxStates( meshState, []( const MeshState& meshState ) -> auto& {
				return meshState.showVertexBinormals;
			} );

			SetupMeshAxisRow( normalCheckboxes, "Normals", [&meshState]( bool checked, uint32_t index ) {
				const auto& foundState = std::find_if( meshState.showVertexNormals.begin(), meshState.showVertexNormals.end(), [index]( const State<std::pair<uint32_t, bool>>& state ) {
					return state.GetValue().first == index;
				} );

				if( foundState != meshState.showVertexNormals.end() )
				{
					foundState->SetValue( { index, checked } );
				}
			} );

			SetupMeshAxisRow( tangentCheckboxes, "Tangents", [&meshState]( bool checked, uint32_t index ) {
				const auto& foundState = std::find_if( meshState.showVertexTangents.begin(), meshState.showVertexTangents.end(), [index]( const State<std::pair<uint32_t, bool>>& state ) {
					return state.GetValue().first == index;
				} );

				if( foundState != meshState.showVertexTangents.end() )
				{
					foundState->SetValue( { index, checked } );
				}
			} );

			SetupMeshAxisRow( binormalCheckboxes, "Binormals", [&meshState]( bool checked, uint32_t index ) {
				const auto& foundState = std::find_if( meshState.showVertexBinormals.begin(), meshState.showVertexBinormals.end(), [index]( const State<std::pair<uint32_t, bool>>& state ) {
					return state.GetValue().first == index;
				} );

				if( foundState != meshState.showVertexBinormals.end() )
				{
					foundState->SetValue( { index, checked } );
				}
			} );

			ImGui::EndTable();
			RenderMorphList( mesh, meshState );
		}
		ImGui::TreePop();
	}
}

void UIGeneralWindow::RenderMorphList( const cmf::Mesh& mesh, MeshState& meshState )
{
	if( ImGui::TreeNode( "##morphs", "Morphs (%zu)", mesh.morphTargets.targets.size() ) )
	{
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted( ICON_FA_MAGNIFYING_GLASS.text );
		ImGui::SameLine();
		m_morphFilter.Draw( "##morphfilter", ImGui::GetContentRegionAvail().x );
		ImGui::SetItemTooltip( "Filter morph targets" );

		const auto& currentLod = mesh.lods[meshState.activeLod.GetValue()];
		for( size_t morphIndex = 0; morphIndex < currentLod.morphTargets.size(); ++morphIndex )
		{
			const auto morphName = cmf::ToStdString( mesh.morphTargets.targets[morphIndex].name );
			if( !m_morphFilter.PassFilter( morphName.c_str() ) )
			{
				continue;
			}

			ImGui::SeparatorText( morphName.c_str() );

			if( ImGui::BeginTable( "##table", 3 ) )
			{
				ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_WidthFixed );
				ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_WidthStretch );
				ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_WidthFixed, UiConsts::BUTTON_WIDTH );
				ImGui::TableNextRow();

				ImGui::TableNextColumn();
				ImGui::Text( "Weight" );
				const auto& morph = currentLod.morphTargets[morphIndex];
				auto& morphState = meshState.morphs[morphIndex];

				auto [weight, enabled] = morphState.GetValue();

				ImGui::TableNextColumn();
				ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x );

				if( ImGui::SliderFloat( ( std::string( "##morphslider" ) + std::to_string( morphIndex ) ).c_str(), &weight, 0.0f, 1.0f ) )
				{
					morphState.SetValue( { weight, enabled } );
				}
				ImGui::TableNextColumn();

				if( ImGui::Checkbox( ( std::string( "##morphcheckbox" ) + std::to_string( morphIndex ) ).c_str(), &enabled ) )
				{
					morphState.SetValue( { weight, enabled } );
				}
				ImGui::EndTable();
			}
		}
		ImGui::TreePop();
	}
}

void UIGeneralWindow::RenderSkeletonList( AppState& appState )
{
	const auto& data = appState.cmfContent.GetValue();
	bool hasSkeletons = data && !data->m_cmfData->skeletons.empty();
	size_t skeletonCount = hasSkeletons ? data->m_cmfData->skeletons.size() : 0;
	std::string header = "Skeletons (" + std::to_string( skeletonCount ) + ")";
	ImGui::SeparatorText( header.c_str() );

	if( ImGui::BeginTable( "##table", 2 ) )
	{
		ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_WidthFixed );
		ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_WidthStretch );

		SetupAttribute( "Display Joints", "Shows skeleton joints", !hasSkeletons, [&]() {
			bool checked = appState.modelState.jointDebug.GetValue();
			if( ImGui::Checkbox( "##displayjoints", &checked ) )
			{
				appState.modelState.jointDebug.SetValue( checked );
			}
		} );

		SetupAttribute( "Display Joint Axes", "Shows skeleton joint axes", !hasSkeletons, [&]() {
			bool checked = appState.modelState.jointAxisDebug.GetValue();
			if( ImGui::Checkbox( "##displayjointaxes", &checked ) )
			{
				appState.modelState.jointAxisDebug.SetValue( checked );
			}
		} );

		SetupAttribute( "Display Bones", "Shows skeleton bones", !hasSkeletons, [&]() {
			bool checked = appState.modelState.boneDebug.GetValue();
			if( ImGui::Checkbox( "##displaybones", &checked ) )
			{
				appState.modelState.boneDebug.SetValue( checked );
			}
		} );
		ImGui::EndTable();
	}
}

void UIGeneralWindow::RenderAnimationOverrideList( AppState& appState )
{
	ImGui::SeparatorText( "Animation Owners" );

	const auto& data = appState.cmfContent.GetValue();
	bool disabled = data == nullptr || data->m_cmfData->skeletons.empty();
	ImGui::BeginDisabled( disabled );

	//  button to add an animation owner from a cmf file
	if( ImGui::Button( "+", ImVec2( ImGui::GetContentRegionAvail().x, UiConsts::BUTTON_HEIGHT ) ) )
	{
		auto* path = ImGui::OpenCmfFileDialog();
		if( path != nullptr )
		{
			auto data = CmfContentLoader::LoadContentFromFile( path );
			if( data )
			{
				appState.modelState.animationOverrides.AddState( data );
				appState.modelState.activeAnimationOwner.SetValue( data );
			}
		}
	}
	if( data == nullptr )
	{
		ImGui::SetItemTooltip( "Adds an animation owner from a cmf file. Disabled because no data is loaded." );
	}
	else if( data->m_cmfData->skeletons.empty() )
	{
		ImGui::SetItemTooltip( "Adds an animation owner from a cmf file. Disabled since model has no skeletons." );
	}
	else
	{
		ImGui::SetItemTooltip( "Adds an animation owner from a cmf file." );
	}

	std::vector<std::shared_ptr<CmfContent>> allModels{};
	if( data )
	{
		allModels.reserve( appState.modelState.animationOverrides.size() + 1 );
		allModels.push_back( data );
	}
	std::transform( appState.modelState.animationOverrides.begin(), appState.modelState.animationOverrides.end(), std::back_inserter( allModels ), []( const auto& state ) { return state.GetValue(); } );

	const auto& skeletonOwners = appState.modelState.animationOverrides;

	uint32_t index = 0;
	for( auto& animationOwner : allModels )
	{
		auto name = std::filesystem::path( animationOwner->m_filePath ).filename().string();
		if( index == 0 )
		{
			name += " (model)";
		}

		if( ImGui::RadioButton( name.c_str(), appState.modelState.activeAnimationOwner.GetValue() == animationOwner ) )
		{
			appState.modelState.activeAnimationOwner.SetValue( animationOwner );
		}

		if( ImGui::BeginItemTooltip() )
		{
			if( index == 0 )
			{
				ImGui::Text( "Model skeleton and animations" );
			}
			else
			{
				ImGui::Text( "Animation owner from %s", animationOwner->m_filePath.c_str() );
			}
			ImGui::Text( "Skeletons" );
			for( const auto& skeleton : animationOwner->m_cmfData->skeletons )
			{
				ImGui::BulletText( "%s has %d bones", cmf::ToStdString( skeleton.name ), skeleton.bones.size() );
			}
			ImGui::EndTooltip();
		}

		if( index != 0 )
		{
			ImGui::SameLine( ImGui::GetContentRegionAvail().x - UiConsts::BUTTON_WIDTH );
			std::string label = "-##" + std::to_string( index );
			if( ImGui::Button( label.c_str() ) )
			{
				appState.modelState.animationOverrides.RemoveAt( index - 1 );

				if( appState.modelState.activeAnimationOwner.GetValue() == animationOwner )
				{
					appState.modelState.activeAnimationOwner.SetValue( appState.cmfContent.GetValue() );
				}
			}
			ImGui::SetItemTooltip( "Removes %s ", name.c_str() );
		}
		++index;
	}
	ImGui::EndDisabled();
}


// NOLINTEND(cppcoreguidelines-pro-type-vararg)
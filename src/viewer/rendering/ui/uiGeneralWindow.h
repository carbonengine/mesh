// Copyright © 2026 CCP ehf.

#pragma once

#include "../../appState.h"
#include <cstdint>
#include <imgui.h>
#include <string>
#include <vector>

// ImGui is using a lot of variadic functions for text formatting, so we disable the cppcoreguidelines-pro-type-vararg lint for this file
// NOLINTBEGIN(cppcoreguidelines-pro-type-vararg)
class UIGeneralWindow
{
public:
	void Render( AppState& appState, float marginTop, float marginBottom );

private:
	void RenderGeneralInfo( AppState& appState );
	void RenderMeshList( AppState& appState );
	void RenderMeshInfo( const cmf::Mesh& mesh, MeshState& meshState );
	void RenderMorphList( const cmf::Mesh& mesh, MeshState& meshState );
	void RenderSkeletonList( AppState& appState );
	void RenderAnimationOverrideList( AppState& appState );

	ImGuiTextFilter m_morphFilter;

	template <typename Callable>
	void SetupAttribute( const char* name, const char* tooltip, bool disabled, Callable constructor )
	{
		ImGui::TableNextRow();

		ImGui::BeginDisabled( disabled );
		ImGui::TableNextColumn();

		ImGui::TextUnformatted( name );
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x );
		constructor();
		if( tooltip )
		{
			ImGui::SetItemTooltip( tooltip );
		}
		ImGui::EndDisabled();
	}
};
// NOLINTEND(cppcoreguidelines-pro-type-vararg)
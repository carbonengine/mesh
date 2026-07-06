// Copyright © 2026 CCP ehf.

#pragma once

#include "../../appState.h"
#include <imgui.h>

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
};
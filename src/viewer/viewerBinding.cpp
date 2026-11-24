// Copyright © 2026 CCP ehf.

#include "viewerBinding.h"

ViewerBinding::ViewerBinding( AppState& appState, SceneRenderer& sceneRenderer, Camera& camera, OrientationGizmoRenderer& gizmo ) :
	m_appState( &appState ),
	m_sceneRenderer( &sceneRenderer ),
	m_camera( &camera ),
	m_gizmo( &gizmo )
{
}

void ViewerBinding::Initialize()
{
	m_appState->windowSize.RegisterCallback( [this]( std::pair<uint32_t, uint32_t> size, AppState& ) {
		auto [width, height] = size;
		m_sceneRenderer->SetRenderSize( width, height );
		m_gizmo->SetSize( width, height );
		m_camera->SetScreenSize( width, height );
	} );

	m_appState->cameraTrigger.RegisterCallback( [this]( CameraTrigger trigger, AppState& state ) {
		m_camera->HandleCameraTriggerChange( trigger, state.cameraFocus.GetValue() );
	} );

	m_appState->mouseState.RegisterCallback( [this]( const MouseState& mouseState, AppState& ) {
		m_camera->HandleMouseStateChanged( mouseState );
	} );

	m_appState->cmfContent.RegisterCallback( [this]( const std::shared_ptr<CmfContent>& cmfContent, AppState& ) {
		if( cmfContent != nullptr )
		{
			m_camera->Reset( cmfContent->GetBoundingSphere() );
		}
	} );

	auto [width, height] = m_appState->windowSize.GetValue();
	m_camera->SetScreenSize( width, height );
}

void ViewerBinding::SetModel( const std::shared_ptr<CmfContent>& data )
{
	// model state callbacks refer to the previous model, they are dropped with the state
	m_appState->ResetModelState();

	ModelRenderable* model = m_sceneRenderer->SetData( data );
	if( model == nullptr )
	{
		return;
	}

	// reset the visualization shader if it is not set or not applicable for the current model
	const auto shaders = SceneRenderer::GetAvailableShaders( *data );
	const auto shader = SceneRenderer::ChooseShader( shaders, m_appState->modelState.activeShader.GetValue().first );
	if( shader.has_value() )
	{
		m_appState->modelState.activeShader.SetValue( *shader );
	}
	m_appState->modelState.availableShaders.SetValue( shaders );

	RegisterModelCallbacks();

	for( size_t meshIndex = 0; meshIndex < model->GetMeshCount(); ++meshIndex )
	{
		AddMeshState( model->GetMesh( meshIndex ) );
	}

	model->SetMorphWeightCallback( [this]( size_t meshIndex, uint32_t morphIndex, float weight ) {
		// reflect animated weights in the UI
		if( meshIndex >= m_appState->modelState.meshes.size() )
		{
			return;
		}
		auto& morphs = m_appState->modelState.meshes[meshIndex].GetValue().morphs;
		if( morphIndex < morphs.size() )
		{
			auto& morphState = morphs[morphIndex];
			morphState.SetValueNoCallback( { weight, morphState.GetValue().second } );
		}
	} );

	m_appState->modelState.activeAnimationOwner.SetValue( data );
}

void ViewerBinding::RegisterModelCallbacks()
{
	auto& modelState = m_appState->modelState;

	modelState.modelBoundingBox.RegisterCallback( [this]( bool value, AppState& ) {
		if( auto* model = m_sceneRenderer->GetModel() )
		{
			model->SetShowBoundingBox( value );
		}
	} );

	// when the active animation owner changes, we need to update the animation state and meshes
	modelState.activeAnimationOwner.RegisterCallback( [this]( const std::shared_ptr<CmfContent>& activeAnimationOwner, AppState& ) {
		if( auto* model = m_sceneRenderer->GetModel() )
		{
			model->SetAnimationOwner( activeAnimationOwner );
		}
	} );

	modelState.currentAnimation.RegisterCallback( [this]( const std::string& animationName, AppState& appState ) {
		auto* model = m_sceneRenderer->GetModel();
		if( model == nullptr )
		{
			return;
		}
		const auto activeAnimationOwner = appState.modelState.activeAnimationOwner.GetValue();

		if( activeAnimationOwner == nullptr )
		{
			Log::Error( "No active animation owner found for animation change." );
			return;
		}

		const auto& animations = activeAnimationOwner->m_cmfData->animations;
		const auto* const animationIt = std::find_if( animations.begin(), animations.end(), [&animationName]( const cmf::Animation& anim ) {
			return cmf::ToStdString( anim.name ) == animationName;
		} );

		if( animationIt == animations.end() )
		{
			if( !animationName.empty() )
			{
				Log::Error( "Animation %s not found in active animation owner.", animationName.c_str() );
			}
			model->SetAnimation( nullptr );
		}
		else
		{
			model->SetAnimation( &*animationIt );
		}
	} );

	// the model handles the animation time, since it needs to update both the animation state and the meshes at the same time
	modelState.currentAnimationTime.RegisterCallback( [this]( float animationTime, AppState& ) {
		if( auto* model = m_sceneRenderer->GetModel() )
		{
			model->SetAnimationTime( animationTime );
		}
	} );

	modelState.selectedBones.RegisterCallback( [this]( const std::vector<uint32_t>& selectedBones, AppState& ) {
		if( auto* model = m_sceneRenderer->GetModel() )
		{
			model->SetSelectedBones( selectedBones );
		}
	} );

	modelState.activeShader.RegisterCallback( [this]( const SceneRenderer::ShaderEntry& shader, AppState& ) {
		if( auto* model = m_sceneRenderer->GetModel() )
		{
			model->SetShader( shader.first, shader.second );
		}
	} );

	modelState.selectedLod.RegisterCallback( [this]( int32_t lodIndex, AppState& appState ) {
		auto* model = m_sceneRenderer->GetModel();
		if( model == nullptr )
		{
			return;
		}
		model->SetLodOverride( lodIndex );
		for( size_t meshIndex = 0; meshIndex < model->GetMeshCount() && meshIndex < appState.modelState.meshes.size(); ++meshIndex )
		{
			appState.modelState.meshes[meshIndex].GetValue().activeLod.SetValueNoCallback( model->GetMesh( meshIndex ).GetCurrentLod() );
		}
	} );
}

void ViewerBinding::AddMeshState( MeshRenderable& mesh )
{
	m_appState->modelState.meshes.AddState( [&mesh]( MeshState& meshState ) {
		const auto& cmfMesh = mesh.GetCmfMesh();
		meshState.display.RegisterCallback( [&mesh]( bool visible, AppState& ) {
			mesh.SetVisible( visible );
		} );
		for( size_t i = 0; i < cmfMesh.areas.size(); ++i )
		{
			meshState.areaDisplay.AddState();
			meshState.areaDisplay[i].RegisterCallback( [&mesh, i]( bool visible, AppState& ) {
				mesh.SetAreaVisible( i, visible );
			} );
		}
		meshState.wireframeOverlay.RegisterCallback( [&mesh]( bool enabled, AppState& ) {
			mesh.SetWireframe( enabled );
		} );
		meshState.audioOcclusionMesh.RegisterCallback( [&mesh]( bool enabled, AppState& ) {
			mesh.SetAudioOcclusion( enabled );
		} );
		meshState.renderBoundingBox.RegisterCallback( [&mesh]( bool enabled, AppState& ) {
			mesh.SetShowBoundingBox( enabled );
		} );
		meshState.activeLod.RegisterCallback( [&mesh]( uint32_t lodIndex, AppState& ) {
			mesh.SetLod( lodIndex );
		} );
		meshState.activeLod.SetValueNoCallback( mesh.GetCurrentLod() );
		for( size_t i = 0; i < cmfMesh.morphTargets.targets.size(); ++i )
		{
			meshState.morphs.AddState();
			meshState.morphs[i].RegisterCallback( [&mesh, i]( std::pair<float, bool> morph, AppState& ) {
				mesh.SetMorphWeight( static_cast<uint32_t>( i ), morph.second ? morph.first : 0.0f );
			} );
		}
		for( const auto usageIndex : mesh.GetNormalAxisUsages() )
		{
			meshState.showVertexNormals.AddState( { usageIndex, false } );
		}
		for( const auto usageIndex : mesh.GetTangentAxisUsages() )
		{
			meshState.showVertexTangents.AddState( { usageIndex, false } );
		}
		for( const auto usageIndex : mesh.GetBinormalAxisUsages() )
		{
			meshState.showVertexBinormals.AddState( { usageIndex, false } );
		}
	} );
}

void ViewerBinding::Sync()
{
	auto* model = m_sceneRenderer->GetModel();
	if( model == nullptr )
	{
		return;
	}
	auto& meshes = m_appState->modelState.meshes;
	const bool autoLod = m_appState->modelState.selectedLod.GetValue() < 0;
	for( size_t meshIndex = 0; meshIndex < model->GetMeshCount() && meshIndex < meshes.size(); ++meshIndex )
	{
		auto& mesh = model->GetMesh( meshIndex );
		auto& meshState = meshes[meshIndex].GetValue();
		if( autoLod )
		{
			meshState.meshScreenSize.SetValueNoCallback( mesh.GetScreenSize() );
			meshState.activeLod.SetValueNoCallback( mesh.GetCurrentLod() );
		}

		// the debug axis toggles are read every frame
		size_t index = 0;
		for( const auto& state : meshState.showVertexNormals )
		{
			mesh.SetShowNormals( index++, state.GetValue().second );
		}
		index = 0;
		for( const auto& state : meshState.showVertexTangents )
		{
			mesh.SetShowTangents( index++, state.GetValue().second );
		}
		index = 0;
		for( const auto& state : meshState.showVertexBinormals )
		{
			mesh.SetShowBinormals( index++, state.GetValue().second );
		}
	}
}

DebugDrawSettings ViewerBinding::GetDebugDrawSettings() const
{
	DebugDrawSettings settings;
	settings.bones = m_appState->modelState.boneDebug.GetValue();
	settings.joints = m_appState->modelState.jointDebug.GetValue();
	settings.jointAxes = m_appState->modelState.jointAxisDebug.GetValue();
	const auto content = m_appState->cmfContent.GetValue();
	if( content != nullptr )
	{
		settings.radius = content->GetBoundingSphere().radius;
	}
	return settings;
}

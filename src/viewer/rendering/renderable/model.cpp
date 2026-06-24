// Copyright © 2026 CCP ehf.

#include "model.h"

#include "../models/boundingBox.h"
#include "../models/primitiveEffects.h"


ModelRenderable::ModelRenderable( std::shared_ptr<CmfContent> data, std::shared_ptr<const Renderer> renderer ) :
	m_cmfContent( data ),
	m_renderer( renderer ),
	m_boundingBox( BoundingBox::Create( renderer, Vector3( 0.5, 0.5, 0.0 ) ) )
{
	CcpMath::AxisAlignedBox combined{};

	uint8_t skeletonIndex = 0;
	for( const auto& skeleton : data->m_cmfData->skeletons )
	{
		m_animationStates.push_back( { skeletonIndex++, skeleton, m_renderer } );
	}

	for( const auto& mesh : data->m_cmfData->meshes )
	{
		combined.IncludeBox( mesh.bounds );
		m_meshes.push_back( { data, mesh, m_renderer } );
	}

	m_boundingSphere = data->GetBoundingSphere();
	m_boundingBoxTransform = ScalingMatrix( combined.Size() ) * TranslationMatrix( combined.Center() );
}

ModelRenderable::~ModelRenderable()
{
	m_meshes.clear();
}

VkResult ModelRenderable::Initialize( AppState& appState )
{
	appState.modelState.modelBoundingBox.RegisterCallback( [&]( bool value, AppState& ) {
		m_showBoundingBox = value;
	} );

	// when the active animation owner changes, we need to update the animation state and meshes
	appState.modelState.activeAnimationOwner.RegisterCallback( [&]( std::shared_ptr<CmfContent> activeAnimationOwner, AppState& appState ) {
		for( auto& mesh : m_meshes )
		{
			mesh.SetAnimation( nullptr );
		}
		for( auto& animationState : m_animationStates )
		{
			animationState.SetAnimationOwner( activeAnimationOwner );
			animationState.SetAnimation( nullptr );
		}
		UpdateAnimation( 0.0f, appState );
	} );

	appState.modelState.currentAnimation.RegisterCallback( [&]( std::string animationName, AppState& appState ) {
		const auto activeAnimationOwner = appState.modelState.activeAnimationOwner.GetValue();

		if( activeAnimationOwner == nullptr )
		{
			Log::Error( "No active animation owner found for animation change." );
			return;
		}

		const auto& animationIt = std::find_if( activeAnimationOwner->m_cmfData->animations.begin(), activeAnimationOwner->m_cmfData->animations.end(), [animationName]( const cmf::Animation& anim ) {
			return cmf::ToStdString( anim.name ) == animationName;
		} );

		if( animationIt == activeAnimationOwner->m_cmfData->animations.end() )
		{
			if( !animationName.empty() )
			{
				Log::Error( "Animation %s not found in active animation owner.", animationName.c_str() );
			}
			for( auto& mesh : m_meshes )
			{
				mesh.SetAnimation( nullptr );
			}
			for( auto& animationState : m_animationStates )
			{
				animationState.SetAnimation( nullptr );
			}
		}
		else
		{
			const auto& animation = *animationIt;

			for( auto& mesh : m_meshes )
			{
				mesh.SetAnimation( &animation );
			}
			for( auto& animationState : m_animationStates )
			{
				animationState.SetAnimation( &animation );
			}
		}
		UpdateAnimation( 0.0f, appState );
	} );

	// make the model handle the animation time, since we need to update both the animation state and the meshes at the same time
	appState.modelState.currentAnimationTime.RegisterCallback( [&]( float animationTime, AppState& appState ) {
		UpdateAnimation( animationTime, appState );
	} );

	appState.modelState.activeAnimationOwner.SetValue( m_cmfContent );


	for( auto& animationState : m_animationStates )
	{
		animationState.Initialize( appState );
	}

	for( auto& mesh : m_meshes )
	{
		mesh.Initialize( appState );
	}

	m_boundingBox.Initialize();

	return VK_SUCCESS;
}

void ModelRenderable::Update( AppState& appState, const Camera& camera )
{
	for( auto& mesh : m_meshes )
	{
		mesh.Update( appState, camera );
	}
}

void ModelRenderable::Render( GraphicsCommandBuffer& commandBuffer, const AppState& state, const Camera& camera )
{
	for( auto& mesh : m_meshes )
	{
		mesh.Render( commandBuffer, state, camera );
	}

	if( m_showBoundingBox )
	{
		auto vertexData = PrimitiveEffects::VertexUBO{ camera.GetProjection(), camera.GetView(), m_boundingBoxTransform, Vector4() };
		m_boundingBox.SetUniformData( 0, vertexData );
		m_boundingBox.Render( commandBuffer );
	}
}


void ModelRenderable::RenderDebug( GraphicsCommandBuffer& commandBuffer, const AppState& appState, const Camera& camera )
{
	for( auto& mesh : m_meshes )
	{
		mesh.RenderDebug( commandBuffer, appState, camera );
	}

	if( m_showBoundingBox )
	{
		auto vertexData = PrimitiveEffects::VertexUBO{ camera.GetProjection(), camera.GetView(), m_boundingBoxTransform, Vector4() };
		m_boundingBox.SetUniformData( 0, vertexData );
		m_boundingBox.Render( commandBuffer );
	}
}

void ModelRenderable::RenderNoDepthDebug( GraphicsCommandBuffer& commandBuffer, const AppState& appState, const Camera& camera )
{
	for( auto& animationState : m_animationStates )
	{
		animationState.RenderNoDepthDebug( commandBuffer, appState, camera );
	}
}

VkResult ModelRenderable::PrepareModel( ComputeCommandBuffer& computeCommandBuffer )
{
	for( auto& mesh : m_meshes )
	{
		mesh.PrepareMesh( computeCommandBuffer );
	}
	return VK_SUCCESS;
}

void ModelRenderable::UpdateAnimation( float animationTime, AppState& appState )
{
	const auto owner = appState.modelState.activeAnimationOwner.GetValue();
	if( owner == nullptr )
	{
		Log::Error( "No active animation owner found for animation update." );
		return;
	}

	for( auto& animationState : m_animationStates )
	{
		animationState.Update( animationTime );

		for( auto& mesh : m_meshes )
		{
			if( mesh.GetSkeletonIndex() == animationState.GetSkeletonIndex() )
			{
				mesh.UpdateMeshCurves( animationTime, animationState.GetAnimation(), appState );
				mesh.SetSkeletonPose( animationState.GetBoneTransforms() );
			}
		}
	}
}

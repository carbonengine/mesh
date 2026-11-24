// Copyright © 2026 CCP ehf.

#include "model.h"

#include "../models/boundingBox.h"
#include "../models/primitiveEffects.h"


namespace
{
constexpr float BOUNDING_BOX_COLOR = 0.5f;
}

ModelRenderable::ModelRenderable( const std::shared_ptr<CmfContent>& data, const std::shared_ptr<const Renderer>& renderer ) :
	m_renderer( renderer ),
	m_cmfContent( data ),
	m_boundingBox( BoundingBox::Create( renderer, Vector3( BOUNDING_BOX_COLOR, BOUNDING_BOX_COLOR, 0.0f ) ) )
{
	CcpMath::AxisAlignedBox combined{};

	uint8_t skeletonIndex = 0;
	for( const auto& skeleton : data->m_cmfData->skeletons )
	{
		m_animationStates.emplace_back( skeletonIndex++, skeleton, m_renderer );
	}

	for( const auto& mesh : data->m_cmfData->meshes )
	{
		combined.IncludeBox( mesh.bounds );
		m_meshes.emplace_back( data, mesh, m_renderer );
	}

	m_boundingSphere = data->GetBoundingSphere();
	m_boundingBoxTransform = ScalingMatrix( combined.Size() ) * TranslationMatrix( combined.Center() );
}

ModelRenderable::~ModelRenderable()
{
	m_meshes.clear();
}

VkResult ModelRenderable::Initialize()
{
	for( auto& animationState : m_animationStates )
	{
		animationState.Initialize();
	}

	for( auto& mesh : m_meshes )
	{
		mesh.Initialize();
	}

	m_boundingBox.Initialize();

	SetAnimationOwner( m_cmfContent );

	return VK_SUCCESS;
}

void ModelRenderable::SetAnimationOwner( std::shared_ptr<CmfContent> owner )
{
	m_animationOwner = std::move( owner );
	for( auto& mesh : m_meshes )
	{
		mesh.SetAnimation( nullptr );
	}
	for( auto& animationState : m_animationStates )
	{
		animationState.SetAnimationOwner( m_animationOwner );
		animationState.SetAnimation( nullptr );
	}
	UpdateAnimation( 0.0f );
}

void ModelRenderable::SetAnimation( const cmf::Animation* animation )
{
	for( auto& mesh : m_meshes )
	{
		mesh.SetAnimation( animation );
	}
	for( auto& animationState : m_animationStates )
	{
		animationState.SetAnimation( animation );
	}
	UpdateAnimation( 0.0f );
}

void ModelRenderable::SetAnimationTime( float animationTime )
{
	UpdateAnimation( animationTime );
}

void ModelRenderable::SetPose( uint8_t skeletonIndex, const cmf::SkeletonPose& pose )
{
	for( auto& animationState : m_animationStates )
	{
		if( animationState.GetSkeletonIndex() != skeletonIndex )
		{
			continue;
		}
		animationState.SetPose( pose );
		for( auto& mesh : m_meshes )
		{
			if( mesh.GetSkeletonIndex() == skeletonIndex )
			{
				mesh.SetSkeletonPose( animationState.GetBoneTransforms() );
			}
		}
	}
}

void ModelRenderable::SetShader( const std::string& name, const GraphicsEffectTypes::ShaderInputDeclaration& declaration )
{
	for( auto& mesh : m_meshes )
	{
		mesh.SetRenderingMode( name, declaration );
	}
}

void ModelRenderable::SetShowBoundingBox( bool show )
{
	m_showBoundingBox = show;
}

void ModelRenderable::SetSelectedBones( const std::vector<uint32_t>& selectedBones )
{
	for( auto& animationState : m_animationStates )
	{
		animationState.SetSelectedBones( selectedBones );
	}
}

void ModelRenderable::SetLodOverride( int32_t lod )
{
	m_lodOverride = lod;
	if( lod < 0 )
	{
		return;
	}
	for( auto& mesh : m_meshes )
	{
		const auto& lods = mesh.GetCmfMesh().lods;
		if( !lods.empty() )
		{
			// the selected lod applies to all meshes, clamp it to the lods this mesh actually has
			mesh.SetLod( std::min( static_cast<uint32_t>( lod ), static_cast<uint32_t>( lods.size() - 1 ) ) );
		}
	}
}

void ModelRenderable::SetMorphWeightCallback( MorphWeightCallback callback )
{
	m_morphWeightCallback = std::move( callback );
}

size_t ModelRenderable::GetMeshCount() const
{
	return m_meshes.size();
}

MeshRenderable& ModelRenderable::GetMesh( size_t index )
{
	return m_meshes[index];
}

const CcpMath::Sphere& ModelRenderable::GetBoundingSphere() const
{
	return m_boundingSphere;
}

const std::shared_ptr<CmfContent>& ModelRenderable::GetContent() const
{
	return m_cmfContent;
}

void ModelRenderable::Update( const Camera& camera )
{
	for( auto& mesh : m_meshes )
	{
		mesh.Update( camera, m_lodOverride < 0 );
	}
}

void ModelRenderable::Render( GraphicsCommandBuffer& commandBuffer, const Camera& camera )
{
	for( auto& mesh : m_meshes )
	{
		mesh.Render( commandBuffer, camera );
	}

	if( m_showBoundingBox )
	{
		auto vertexData = PrimitiveEffects::VertexUBO{ camera.GetProjection(), camera.GetView(), m_boundingBoxTransform, Vector4() };
		m_boundingBox.SetUniformData( 0, vertexData );
		m_boundingBox.Render( commandBuffer );
	}
}


void ModelRenderable::RenderDebug( GraphicsCommandBuffer& commandBuffer, const Camera& camera )
{
	for( auto& mesh : m_meshes )
	{
		mesh.RenderDebug( commandBuffer, camera );
	}

	if( m_showBoundingBox )
	{
		auto vertexData = PrimitiveEffects::VertexUBO{ camera.GetProjection(), camera.GetView(), m_boundingBoxTransform, Vector4() };
		m_boundingBox.SetUniformData( 0, vertexData );
		m_boundingBox.Render( commandBuffer );
	}
}

void ModelRenderable::RenderNoDepthDebug( GraphicsCommandBuffer& commandBuffer, const Camera& camera, const DebugDrawSettings& settings )
{
	for( auto& animationState : m_animationStates )
	{
		animationState.RenderNoDepthDebug( commandBuffer, camera, settings );
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

void ModelRenderable::UpdateAnimation( float animationTime )
{
	if( m_animationOwner == nullptr )
	{
		Log::Error( "No active animation owner found for animation update." );
		return;
	}

	for( auto& animationState : m_animationStates )
	{
		animationState.Update( animationTime );

		for( size_t meshIndex = 0; meshIndex < m_meshes.size(); ++meshIndex )
		{
			auto& mesh = m_meshes[meshIndex];
			if( mesh.GetSkeletonIndex() == animationState.GetSkeletonIndex() )
			{
				mesh.UpdateMeshCurves( animationTime, animationState.GetAnimation(), [this, meshIndex]( uint32_t morphIndex, float weight ) {
					if( m_morphWeightCallback )
					{
						m_morphWeightCallback( meshIndex, morphIndex, weight );
					}
				} );
				mesh.SetSkeletonPose( animationState.GetBoneTransforms() );
			}
		}
	}
}

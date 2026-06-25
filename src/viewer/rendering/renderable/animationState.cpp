// Copyright © 2026 CCP ehf.

#include "animationState.h"

#include <numeric>

#include "../models/bones.h"
#include "../models/axis.h"
#include "../models/primitiveEffects.h"


AnimationState::AnimationState( uint8_t m_skeletonIndex, const cmf::Skeleton& skeleton, std::shared_ptr<const Renderer> renderer ) :
	m_skeletonIndex( m_skeletonIndex ),
	m_boneRenderable( std::make_unique<PrimitiveRenderable>( Bones::CreateBone( renderer, skeleton ) ) ),
	m_jointRenderable( std::make_unique<PrimitiveRenderable>( Bones::CreateJoint( renderer, skeleton ) ) ),
	m_axisRenderable( std::make_unique<PrimitiveRenderable>( Axis::CreateOrientationPrimitive( renderer ) ) ),
	m_skeleton( skeleton )
{
	ClearBoneMapping();
	m_boneTransforms.fill( IdentityMatrix() );
	SetAnimation( nullptr );
}

void AnimationState::Initialize( AppState& appState )
{
	appState.modelState.selectedBones.RegisterCallback( [&]( std::vector<uint32_t> selectedBones, AppState& appState ) {
		SetSelectedBones( selectedBones );
	} );

	m_axisRenderable->Initialize();
	m_boneRenderable->Initialize();
	m_jointRenderable->Initialize();
}

void AnimationState::SetAnimationOwner( std::shared_ptr<CmfContent> owner )
{
	m_animationOverrideSkeleton.reset();
	ClearBoneMapping();
	// when we set a new animation owner, we need to check if the skeleton of the new animation is different from the model skeleton, and if so, we need to create a new animation override skeleton and create a new bone mapping
	if( owner && owner->m_cmfData->skeletons.size() > m_skeletonIndex )
	{
		const auto& newSkeleton = owner->m_cmfData->skeletons[m_skeletonIndex];
		m_animationOverrideSkeleton = std::make_unique<cmf::Skeleton>( newSkeleton );
		CreateBoneMapping();
	}
}

void AnimationState::SetAnimation( const cmf::Animation* animation )
{
	const cmf::Skeleton& animationSkeleton = m_animationOverrideSkeleton ? *m_animationOverrideSkeleton : m_skeleton;
	cmf::RestPose( m_pose, animationSkeleton );
	m_animation = animation;
	if( animation )
	{
		m_animationPlayer = std::make_unique<cmf::AnimationPlayer>( animationSkeleton, *animation );
	}
	else
	{
		m_animationPlayer.reset();
	}
	Update( 0.0f );
}

void AnimationState::SetSelectedBones( const std::vector<uint32_t>& selectedBones )
{
	std::vector<uint32_t> boneList = std::vector<uint32_t>( 0xFF, 0 );
	for( const auto& boneIndex : selectedBones )
	{
		boneList[boneIndex] = 1;
	}

	m_boneRenderable->GetEffect().SetStorageBufferData( 3, boneList.data(), boneList.size() );
	m_jointRenderable->GetEffect().SetStorageBufferData( 3, boneList.data(), boneList.size() );
}

void AnimationState::Update( float animationTime )
{
	if( m_animationPlayer )
	{
		m_animationPlayer->Sample( m_pose, animationTime );
	}

	cmf::ComputeWorldTransforms( m_boneWorldTransforms, m_pose );
	std::vector<Matrix> sortedBoneWorldTransforms( m_skeleton.bones.size() );

	for( uint32_t boneIdx = 0; boneIdx < m_skeleton.bones.size(); ++boneIdx )
	{
		auto worldTransform = IdentityMatrix();
		auto invBindTransform = IdentityMatrix();

		if( boneIdx < m_modelBoneToAnimationBoneMapping.size() )
		{
			auto skeletonIndex = m_modelBoneToAnimationBoneMapping[boneIdx];
			if( skeletonIndex >= 0 && skeletonIndex < m_boneWorldTransforms.size() )
			{
				worldTransform = m_boneWorldTransforms[skeletonIndex];
			}
		}

		invBindTransform = m_skeleton.invBindTransforms[boneIdx];

		m_boneTransforms[boneIdx] = invBindTransform * worldTransform;

		sortedBoneWorldTransforms[boneIdx] = worldTransform;
	}

	m_boneRenderable->GetEffect().SetStorageBufferData( 1, sortedBoneWorldTransforms.data(), sortedBoneWorldTransforms.size() );
	m_jointRenderable->GetEffect().SetStorageBufferData( 1, sortedBoneWorldTransforms.data(), sortedBoneWorldTransforms.size() );
	m_axisRenderable->GetEffect().SetStorageBufferData( 1, sortedBoneWorldTransforms.data(), sortedBoneWorldTransforms.size() );
}

void AnimationState::RenderNoDepthDebug( GraphicsCommandBuffer& commandBuffer, const AppState& appState, const Camera& camera )
{
	bool showBones = appState.modelState.boneDebug.GetValue();
	bool showJoints = appState.modelState.jointDebug.GetValue();
	bool showJointAxis = appState.modelState.jointAxisDebug.GetValue();

	if( !showBones && !showJoints && !showJointAxis )
	{
		return;
	}

	auto radius = appState.cmfContent.GetValue()->GetBoundingSphere().radius;

	auto perObjectData = PrimitiveEffects::VertexUBO{
		camera.GetProjection(),
		camera.GetView(),
		IdentityMatrix(),
		Vector4{ 0.0f, 0.0f, 0.0f, 0.0f }
	};
	std::vector<uint8_t> renderedSkeletons;

	uint32_t boneCount = (uint32_t)m_boneWorldTransforms.size();
	perObjectData.boneInfo.x = (float)boneCount;
	if( showBones )
	{
		perObjectData.boneInfo.y = 2.0f; // 2 bones per instance
		m_boneRenderable->SetUniformData( 0, perObjectData );
		m_boneRenderable->Render( commandBuffer, boneCount );
	}
	if( showJoints )
	{
		float size = radius / 100.0f;
		perObjectData.model = ScalingMatrix( size, size, size );
		perObjectData.boneInfo.y = 1.0f; // 1 bone per instance
		m_jointRenderable->SetUniformData( 0, perObjectData );
		m_jointRenderable->Render( commandBuffer, boneCount );
	}
	if( showJointAxis )
	{
		float size = radius / 50.0f;
		perObjectData.model = ScalingMatrix( size, size, size );
		perObjectData.boneInfo.y = 1.0f; // 1 bone per instance

		m_axisRenderable->SetUniformData( 0, perObjectData );
		m_axisRenderable->Render( commandBuffer, boneCount );
	}
}

// creates a bone mapping from the model skeleton to the animation skeleton
void AnimationState::CreateBoneMapping()
{
	m_modelBoneToAnimationBoneMapping.resize( m_skeleton.bones.size(), -1 );
	uint32_t meshBoneIndex = 0;
	for( const auto& skeletonBoneName : m_skeleton.bones )
	{
		auto foundBone = std::find_if( m_animationOverrideSkeleton->bones.begin(), m_animationOverrideSkeleton->bones.end(), [skeletonBoneName]( cmf::String boneName ) {
			return skeletonBoneName == boneName;
		} );
		if( foundBone != m_animationOverrideSkeleton->bones.end() )
		{
			m_modelBoneToAnimationBoneMapping[meshBoneIndex] = (int32_t)std::distance( m_animationOverrideSkeleton->bones.begin(), foundBone );
		}
		++meshBoneIndex;
	}
}

void AnimationState::ClearBoneMapping()
{
	// This will clear the mapping and have a 1-to-1 mapping from the model skeleton to the animation skeleton, which is the default when we don't have an animation override
	// i.e element 0 will point to bone 0, element 1 will point to bone 1, etc.
	m_modelBoneToAnimationBoneMapping.resize( m_skeleton.bones.size() );
	std::iota( std::begin( m_modelBoneToAnimationBoneMapping ), std::end( m_modelBoneToAnimationBoneMapping ), 0 );
}

uint8_t AnimationState::GetSkeletonIndex() const
{
	return m_skeletonIndex;
}

const cmf::Animation* AnimationState::GetAnimation() const
{
	return m_animation;
}

const std::array<Matrix, 0xFF>& AnimationState::GetBoneTransforms() const
{
	return m_boneTransforms;
}

const cmf::Span<cmf::String>& AnimationState::GetBoneNames() const
{
	return m_skeleton.bones;
}

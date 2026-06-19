// Copyright © 2026 CCP ehf.

#include "gtest/gtest.h"

#include "cmf/animation.h"
#include "cmf/memallocator.h"
#include "cmf/transforms.h"


namespace
{

template <typename T>
cmf::AnimationCurve MakeInPlaceCurve( const std::vector<float>& knots, const std::vector<T>& values, cmf::Interpolation interpolation, cmf::MemoryAllocator& allocator )
{
	cmf::AnimationCurve result;
	result.valueDimension = sizeof( T ) / sizeof( float );
	result.interpolation = interpolation;
	result.knotType = cmf::ElementType::Float32;
	result.valueType = cmf::ElementType::Float32;
	result.knotCount = uint32_t( knots.size() );
	result.knots = allocator.AllocateSpan<uint8_t>( knots.size() * sizeof( float ) );
	memcpy( result.knots.ptr, knots.data(), knots.size() * sizeof( float ) );
	result.values = allocator.AllocateSpan<uint8_t>( values.size() * sizeof( T ) );
	memcpy( result.values.ptr, values.data(), values.size() * sizeof( T ) );
	return result;
}


struct BoneDesc
{
	const char* name;
	uint32_t parentIndex;
	cmf::Transform restTransform;
};

cmf::Skeleton MakeInPlaceSkeleton( const std::vector<BoneDesc>& bones, cmf::MemoryAllocator& allocator )
{
	cmf::Skeleton result;
	for( const auto& bone : bones )
	{
		cmf::Modify( result.bones, allocator ).push_back( allocator.AllocateString( bone.name ) );
		cmf::Modify( result.parents, allocator ).push_back( bone.parentIndex );
		cmf::Modify( result.restTransforms, allocator ).push_back( bone.restTransform );
		if( bone.parentIndex == 0xffffffff )
		{
			cmf::Modify( result.invBindTransforms, allocator ).push_back( Inverse( TransformationMatrix( bone.restTransform.scale, bone.restTransform.rotation, bone.restTransform.position ) ) );
		}
		else
		{
			cmf::Modify( result.invBindTransforms, allocator ).push_back( result.invBindTransforms[bone.parentIndex] * Inverse( TransformationMatrix( bone.restTransform.scale, bone.restTransform.rotation, bone.restTransform.position ) ) );
		}
	}
	return result;
}

const Vector3 TEST_ANIMATION_CHILD_POSITION0 = Vector3( 0.0f, 1.f, 2.f );
const Vector3 TEST_ANIMATION_CHILD_POSITION1 = Vector3( 100.0f, 110.f, 120.f );

Vector3 GetTestAnimationBonePosition( float localTime )
{
	return TEST_ANIMATION_CHILD_POSITION0 * ( 1 - localTime ) + TEST_ANIMATION_CHILD_POSITION1 * localTime;
}

cmf::AnimationPlayer MakeTestAnimationPlayer( cmf::MemoryAllocator& allocator )
{
	auto skeleton = MakeInPlaceSkeleton( {
											 { "Root", 0xffffffff, { { 0.f, 0.f, 0.f }, { 0.f, 0.f, 0.f, 1.f }, { 1.f, 1.f, 1.f } } },
											 { "Child", 0, { { 1.f, 0.f, 0.f }, { 0.f, 0.f, 0.f, 1.f }, { 1.f, 1.f, 1.f } } },
										 },
										 allocator );
	auto skeletonPtr = new( allocator.Allocate( sizeof( cmf::Skeleton ) ) ) cmf::Skeleton( skeleton );

	auto curve = MakeInPlaceCurve<Vector3>( { 0.0f, 1.0f }, { TEST_ANIMATION_CHILD_POSITION0, TEST_ANIMATION_CHILD_POSITION1 }, cmf::Interpolation::Linear, allocator );

	cmf::AnimationChannel channel;
	channel.targetType = cmf::AnimationChannelTargetType::BonePosition;
	channel.target = allocator.AllocateString( "Child" );
	channel.curveIndex = 0;

	auto animation = new( allocator.Allocate( sizeof( cmf::Animation ) ) ) cmf::Animation();
	cmf::Modify( animation->channels, allocator ).push_back( channel );
	animation->duration = 1.f;

	cmf::Modify( animation->curves, allocator ).push_back( curve );

	return cmf::AnimationPlayer( *skeletonPtr, *animation );
}

bool NearlyEqual( const Vector3& a, const Vector3& b, float epsilon = 0.001f )
{
	return std::abs( a.x - b.x ) < epsilon && std::abs( a.y - b.y ) < epsilon && std::abs( a.z - b.z ) < epsilon;
}

bool NearlyEqual( const Quaternion& a, const Quaternion& b, float epsilon = 0.001f )
{
	return std::abs( a.x - b.x ) < epsilon && std::abs( a.y - b.y ) < epsilon && std::abs( a.z - b.z ) < epsilon && std::abs( a.w - b.w ) < epsilon;
}

}

TEST( Curves, CanSampleConstantCurve )
{
	cmf::MemoryAllocator allocator;

	auto curve = MakeInPlaceCurve<float>( { 1.0f }, { 123.0f }, cmf::Interpolation::Step, allocator );

	EXPECT_EQ( cmf::SampleScalarCurve( curve, 0.5f ), 123.f );
	EXPECT_EQ( cmf::SampleScalarCurve( curve, 1.0f ), 123.f );
	EXPECT_EQ( cmf::SampleScalarCurve( curve, 2.0f ), 123.f );
}

TEST( Curves, CanSampleLinearCurve )
{
	cmf::MemoryAllocator allocator;

	auto curve = MakeInPlaceCurve<float>( { 0.0f, 1.0f }, { 0.0f, 100.0f }, cmf::Interpolation::Linear, allocator );
	EXPECT_EQ( cmf::SampleScalarCurve( curve, -1.0f ), 0.f );
	EXPECT_EQ( cmf::SampleScalarCurve( curve, 0.0f ), 0.f );
	EXPECT_EQ( cmf::SampleScalarCurve( curve, 0.5f ), 50.f );
	EXPECT_EQ( cmf::SampleScalarCurve( curve, 1.0f ), 100.f );
	EXPECT_EQ( cmf::SampleScalarCurve( curve, 2.0f ), 100.f );
}

TEST( Curves, CanSampleStepCurve )
{
	cmf::MemoryAllocator allocator;

	auto curve = MakeInPlaceCurve<float>( { 0.0f, 1.0f }, { 0.0f, 100.0f }, cmf::Interpolation::Step, allocator );
	EXPECT_EQ( cmf::SampleScalarCurve( curve, -1.0f ), 0.f );
	EXPECT_EQ( cmf::SampleScalarCurve( curve, 0.0f ), 0.f );
	EXPECT_EQ( cmf::SampleScalarCurve( curve, 0.5f ), 0.f );
	EXPECT_EQ( cmf::SampleScalarCurve( curve, 1.0f ), 100.f );
	EXPECT_EQ( cmf::SampleScalarCurve( curve, 2.0f ), 100.f );
}

TEST( Curves, CanSampleComplexLinearCurve )
{
	cmf::MemoryAllocator allocator;

	auto curve = MakeInPlaceCurve<float>( { 0.0f, 1.0f, 2.0f }, { 0.0f, 100.0f, 50.f }, cmf::Interpolation::Linear, allocator );
	EXPECT_EQ( cmf::SampleScalarCurve( curve, -1.0f ), 0.f );
	EXPECT_EQ( cmf::SampleScalarCurve( curve, 0.0f ), 0.f );
	EXPECT_EQ( cmf::SampleScalarCurve( curve, 0.5f ), 50.f );
	EXPECT_EQ( cmf::SampleScalarCurve( curve, 1.0f ), 100.f );
	EXPECT_EQ( cmf::SampleScalarCurve( curve, 1.5f ), 75.f );
	EXPECT_EQ( cmf::SampleScalarCurve( curve, 2.0f ), 50.f );
	EXPECT_EQ( cmf::SampleScalarCurve( curve, 3.0f ), 50.f );
}

TEST( Curves, CanHandleZeroLengthSegments )
{
	cmf::MemoryAllocator allocator;

	auto curve = MakeInPlaceCurve<float>( { 0.0f, 1.0f, 1.0f }, { 0.0f, 100.0f, 50.f }, cmf::Interpolation::Linear, allocator );
	EXPECT_EQ( cmf::SampleScalarCurve( curve, -1.0f ), 0.f );
	EXPECT_EQ( cmf::SampleScalarCurve( curve, 0.0f ), 0.f );
	EXPECT_EQ( cmf::SampleScalarCurve( curve, 0.5f ), 50.f );
	EXPECT_EQ( cmf::SampleScalarCurve( curve, 0.75f ), 75.f );
	EXPECT_EQ( cmf::SampleScalarCurve( curve, 1.0f ), 50.f );
	EXPECT_EQ( cmf::SampleScalarCurve( curve, 1.5f ), 50.f );
}

TEST( Curves, CanSampleVector3Curve )
{
	cmf::MemoryAllocator allocator;

	auto curve = MakeInPlaceCurve<Vector3>( { 0.0f, 1.0f }, { { 0.0f, 1.f, 2.f }, { 100.0f, 110.0f, 120.0f } }, cmf::Interpolation::Linear, allocator );
	EXPECT_EQ( cmf::SampleVector3Curve( curve, -1.0f ), Vector3( 0.f, 1.f, 2.f ) );
	EXPECT_EQ( cmf::SampleVector3Curve( curve, 0.0f ), Vector3( 0.f, 1.f, 2.f ) );
	EXPECT_EQ( cmf::SampleVector3Curve( curve, 0.5f ), Vector3( 50.f, 1.0f + ( 110.0f - 1.0f ) / 2.f, 2.0f + ( 120.0f - 2.0f ) / 2.f ) );
	EXPECT_EQ( cmf::SampleVector3Curve( curve, 1.0f ), Vector3( 100.f, 110.f, 120.f ) );
	EXPECT_EQ( cmf::SampleVector3Curve( curve, 1.5f ), Vector3( 100.f, 110.f, 120.f ) );
}

TEST( Curves, CanSampleQuaternionCurve )
{
	cmf::MemoryAllocator allocator;

	auto curve = MakeInPlaceCurve<Quaternion>( { 0.0f, 1.0f }, { { 0.f, 0.f, 0.f, 1.f }, { 0.f, 1.f, 0.f, 0.f } }, cmf::Interpolation::Linear, allocator );
	EXPECT_EQ( cmf::SampleQuaternionCurve( curve, -1.0f ), Quaternion( 0.f, 0.f, 0.f, 1.f ) );
	EXPECT_EQ( cmf::SampleQuaternionCurve( curve, 0.0f ), Quaternion( 0.f, 0.f, 0.f, 1.f ) );
	EXPECT_EQ( cmf::SampleQuaternionCurve( curve, 0.5f ), Slerp( Quaternion( 0.f, 0.f, 0.f, 1.f ), Quaternion( 0.f, 1.f, 0.f, 0.f ), 0.5f ) );
	EXPECT_EQ( cmf::SampleQuaternionCurve( curve, 0.75f ), Slerp( Quaternion( 0.f, 0.f, 0.f, 1.f ), Quaternion( 0.f, 1.f, 0.f, 0.f ), 0.75f ) );
	EXPECT_EQ( cmf::SampleQuaternionCurve( curve, 1.0f ), Quaternion( 0.f, 1.f, 0.f, 0.f ) );
	EXPECT_EQ( cmf::SampleQuaternionCurve( curve, 1.5f ), Quaternion( 0.f, 1.f, 0.f, 0.f ) );
}

TEST( Animation, CanCreateRestPose )
{
	cmf::MemoryAllocator allocator;

	auto skeleton = MakeInPlaceSkeleton( {
											 { "Root", 0xffffffff, { { 0.f, 0.f, 0.f }, { 0.f, 0.f, 0.f, 1.f }, { 1.f, 1.f, 1.f } } },
											 { "Child", 0, { { 1.f, 0.f, 0.f }, { 0.f, 0.f, 0.f, 1.f }, { 1.f, 1.f, 1.f } } },
										 },
										 allocator );
	cmf::SkeletonPose pose;
	cmf::RestPose( pose, skeleton );
	EXPECT_EQ( pose.skeleton, &skeleton );
	EXPECT_EQ( pose.boneTransforms.size(), 2 );
	EXPECT_EQ( pose.boneTransforms[0].position, Vector3( 0.f, 0.f, 0.f ) );
	EXPECT_EQ( pose.boneTransforms[0].rotation, Quaternion( 0.f, 0.f, 0.f, 1.f ) );
	EXPECT_EQ( pose.boneTransforms[0].scale, Vector3( 1.f, 1.f, 1.f ) );
	EXPECT_EQ( pose.boneTransforms[1].position, Vector3( 1.f, 0.f, 0.f ) );
	EXPECT_EQ( pose.boneTransforms[1].rotation, Quaternion( 0.f, 0.f, 0.f, 1.f ) );
	EXPECT_EQ( pose.boneTransforms[1].scale, Vector3( 1.f, 1.f, 1.f ) );
}



TEST( Animation, CanSampleAnimation )
{
	cmf::MemoryAllocator allocator;

	auto skeleton = MakeInPlaceSkeleton( {
											 { "Root", 0xffffffff, { { 0.f, 0.f, 0.f }, { 0.f, 0.f, 0.f, 1.f }, { 1.f, 1.f, 1.f } } },
											 { "Child", 0, { { 1.f, 0.f, 0.f }, { 0.f, 0.f, 0.f, 1.f }, { 1.f, 1.f, 1.f } } },
										 },
										 allocator );

	auto curve = MakeInPlaceCurve<Vector3>( { 0.0f, 1.0f }, { { 0.0f, 1.f, 2.f }, { 100.0f, 110.0f, 120.0f } }, cmf::Interpolation::Linear, allocator );

	cmf::AnimationChannel channel;
	channel.targetType = cmf::AnimationChannelTargetType::BonePosition;
	channel.target = allocator.AllocateString( "Child" );
	channel.curveIndex = 0;

	cmf::Animation animation;
	cmf::Modify( animation.channels, allocator ).push_back( channel );

	cmf::Span<cmf::AnimationCurve> curves;
	cmf::Modify( curves, allocator ).push_back( curve );

	cmf::SkeletonPose pose;
	cmf::RestPose( pose, skeleton );
	cmf::SampleAnimation( pose, animation, curves, 0.5f );
	// Child position should be animated
	EXPECT_EQ( pose.boneTransforms[1].position, Vector3( 50.f, 1.0f + ( 110.0f - 1.0f ) / 2.f, 2.0f + ( 120.0f - 2.0f ) / 2.f ) );
	// Child rotation and scale should be unchanged
	EXPECT_EQ( pose.boneTransforms[1].rotation, Quaternion( 0.f, 0.f, 0.f, 1.f ) );
	EXPECT_EQ( pose.boneTransforms[1].scale, Vector3( 1.f, 1.f, 1.f ) );
	// Root bone should be unchanged
	EXPECT_EQ( pose.boneTransforms[0].position, Vector3( 0.f, 0.f, 0.f ) );
	EXPECT_EQ( pose.boneTransforms[0].rotation, Quaternion( 0.f, 0.f, 0.f, 1.f ) );
	EXPECT_EQ( pose.boneTransforms[0].scale, Vector3( 1.f, 1.f, 1.f ) );
}

TEST( AnimationPlayer, GetDurationLeftAccountsForSpeed )
{
	cmf::MemoryAllocator allocator;
	cmf::AnimationPlayer player = MakeTestAnimationPlayer( allocator );
	player.SetStartTime( 3.0f );
	player.SetLoopCount( 1 );
	player.SetSpeed( 0.5f );

	EXPECT_EQ( player.GetDurationLeft( 3.0f + 1.5f ), 0.5f );
}

TEST( AnimationPlayer, CanGetLocalTime )
{
	cmf::MemoryAllocator allocator;
	cmf::AnimationPlayer player = MakeTestAnimationPlayer( allocator );
	player.SetStartTime( 3.0f );
	player.SetLoopCount( 10 );
	player.SetSpeed( 0.5f );

	EXPECT_EQ( player.GetLocalTime( 3.0f + 2.5f ), 0.25f );
}

TEST( AnimationPlayer, CanGetLoopIndex )
{
	cmf::MemoryAllocator allocator;
	cmf::AnimationPlayer player = MakeTestAnimationPlayer( allocator );
	player.SetStartTime( 3.0f );
	player.SetLoopCount( 10 );
	player.SetSpeed( 0.5f );

	EXPECT_EQ( player.GetLoopIndex( 1.0f ), -1 );
	EXPECT_EQ( player.GetLoopIndex( 3.5f ), 0 );
	EXPECT_EQ( player.GetLoopIndex( 4.5f ), 0 );
	EXPECT_EQ( player.GetLoopIndex( 6.0f ), 1 );
}

TEST( AnimationPlayer, CanGetInfiniteLoopIndex )
{
	cmf::MemoryAllocator allocator;
	cmf::AnimationPlayer player = MakeTestAnimationPlayer( allocator );
	player.SetStartTime( 3.0f );
	player.SetLoopCount( 0 );
	player.SetSpeed( 0.5f );

	EXPECT_EQ( player.GetLoopIndex( 0.0f ), -1 );
	EXPECT_EQ( player.GetLoopIndex( 1.0f ), -1 );
	EXPECT_EQ( player.GetLoopIndex( 3.5f ), 0 );
	EXPECT_EQ( player.GetLoopIndex( 4.5f ), 0 );
	EXPECT_EQ( player.GetLoopIndex( 6.0f ), 1 );
}

TEST( AnimationPlayer, CanSampleAnimation )
{
	cmf::MemoryAllocator allocator;
	cmf::AnimationPlayer player = MakeTestAnimationPlayer( allocator );
	player.SetStartTime( 3.0f );
	player.SetLoopCount( 1 );
	player.SetSpeed( 0.5f );

	cmf::SkeletonPose pose;
	cmf::RestPose( pose, player.GetSkeleton() );
	EXPECT_TRUE( player.Sample( pose, 3.0f + 0.5f ) );

	EXPECT_EQ( pose.boneTransforms[1].position, GetTestAnimationBonePosition( 0.5f * 0.5f ) );
}

TEST( AnimationPlayer, AnimationDoesNotApplyBeforeStart )
{
	cmf::MemoryAllocator allocator;
	cmf::AnimationPlayer player = MakeTestAnimationPlayer( allocator );
	player.SetStartTime( 123.0f );
	player.SetLoopCount( 1 );

	cmf::SkeletonPose pose;
	cmf::RestPose( pose, player.GetSkeleton() );
	EXPECT_FALSE( player.Sample( pose, 123.0f - 1.0f ) );
}

TEST( AnimationPlayer, AnimationAppliesBeforeStartWithExtrapolateBefore )
{
	cmf::MemoryAllocator allocator;
	cmf::AnimationPlayer player = MakeTestAnimationPlayer( allocator );
	player.SetStartTime( 123.0f );
	player.SetLoopCount( 1 );
	player.SetExtrapolateBefore( true );

	cmf::SkeletonPose pose;
	cmf::RestPose( pose, player.GetSkeleton() );
	EXPECT_TRUE( player.Sample( pose, 123.0f - 0.5f ) );

	EXPECT_EQ( pose.boneTransforms[1].position, TEST_ANIMATION_CHILD_POSITION0 );
}

TEST( AnimationPlayer, AnimationDoesNotApplyAfterStop )
{
	cmf::MemoryAllocator allocator;
	cmf::AnimationPlayer player = MakeTestAnimationPlayer( allocator );
	player.SetStartTime( 123.0f );
	player.SetLoopCount( 1 );

	cmf::SkeletonPose pose;
	cmf::RestPose( pose, player.GetSkeleton() );
	EXPECT_FALSE( player.Sample( pose, 123.0f + 1.0f ) );
}

TEST( AnimationPlayer, AnimationAppliesAfterStopWithExtrapolateAfter )
{
	cmf::MemoryAllocator allocator;
	cmf::AnimationPlayer player = MakeTestAnimationPlayer( allocator );
	player.SetStartTime( 123.0f );
	player.SetLoopCount( 1 );
	player.SetExtrapolateAfter( true );

	cmf::SkeletonPose pose;
	cmf::RestPose( pose, player.GetSkeleton() );
	EXPECT_TRUE( player.Sample( pose, 123.0f + 1.5f ) );

	EXPECT_EQ( pose.boneTransforms[1].position, TEST_ANIMATION_CHILD_POSITION1 );
}

TEST( Animation, BlendAdditivePoseWithZeroAlphaIsIdentity )
{
	cmf::MemoryAllocator allocator;

	auto skeleton = MakeInPlaceSkeleton( {
											 { "Root", 0xffffffff, { { 0.f, 2.f, 0.f }, { 0.f, 0.3826834f, 0.f, 0.9238795f }, { 2.f, 2.f, 2.f } } },
											 { "Child", 0, { { 1.f, 0.5f, 0.f }, { 0.f, 0.f, 0.3826834f, 0.9238795f }, { 1.5f, 1.5f, 1.5f } } },
										 },
										 allocator );

	cmf::SkeletonPose basePose, additivePose, inputPose, outPose;
	cmf::RestPose( basePose, skeleton );
	cmf::RestPose( additivePose, skeleton );
	cmf::RestPose( inputPose, skeleton );

	// Make inputPose differ from basePose
	inputPose.boneTransforms[0].position = Vector3( 3.f, 4.f, 5.f );
	inputPose.boneTransforms[0].rotation = Quaternion( 0.f, 0.f, 0.f, 1.f );
	inputPose.boneTransforms[0].scale = Vector3( 1.f, 1.f, 1.f );
	inputPose.boneTransforms[1].position = Vector3( 2.f, 3.f, 4.f );
	inputPose.boneTransforms[1].rotation = Quaternion( 0.f, 0.7071068f, 0.f, 0.7071068f );
	inputPose.boneTransforms[1].scale = Vector3( 2.f, 2.f, 2.f );

	additivePose.boneTransforms[1].position = Vector3( 5.f, 10.f, 15.f );

	cmf::BlendAdditivePose( outPose, inputPose, basePose, additivePose, 0.0f );

	EXPECT_EQ( outPose.boneTransforms[0].position, inputPose.boneTransforms[0].position );
	EXPECT_EQ( outPose.boneTransforms[0].rotation, inputPose.boneTransforms[0].rotation );
	EXPECT_EQ( outPose.boneTransforms[0].scale, inputPose.boneTransforms[0].scale );
	EXPECT_TRUE( NearlyEqual( outPose.boneTransforms[1].position, inputPose.boneTransforms[1].position ) );
	EXPECT_TRUE( NearlyEqual( outPose.boneTransforms[1].rotation, inputPose.boneTransforms[1].rotation ) );
	EXPECT_TRUE( NearlyEqual( outPose.boneTransforms[1].scale, inputPose.boneTransforms[1].scale ) );
}

TEST( Animation, BlendAdditivePoseWithFullAlpha )
{
	cmf::MemoryAllocator allocator;

	auto skeleton = MakeInPlaceSkeleton( {
											 { "Root", 0xffffffff, { { 0.f, 2.f, 0.f }, { 0.f, 0.3826834f, 0.f, 0.9238795f }, { 2.f, 2.f, 2.f } } },
											 { "Child", 0, { { 1.f, 0.5f, 0.f }, { 0.f, 0.f, 0.3826834f, 0.9238795f }, { 1.5f, 1.5f, 1.5f } } },
										 },
										 allocator );

	cmf::SkeletonPose basePose, additivePose, inputPose, outPose;
	cmf::RestPose( basePose, skeleton );
	cmf::RestPose( additivePose, skeleton );
	cmf::RestPose( inputPose, skeleton );

	// Make inputPose differ from basePose
	inputPose.boneTransforms[1].position = Vector3( 2.f, 3.f, 4.f );
	inputPose.boneTransforms[1].rotation = Quaternion( 0.f, 0.7071068f, 0.f, 0.7071068f );
	inputPose.boneTransforms[1].scale = Vector3( 2.f, 2.f, 2.f );

	additivePose.boneTransforms[1].position = Vector3( 5.f, 0.f, 0.f );
	additivePose.boneTransforms[1].rotation = Quaternion( 0.f, 0.7071068f, 0.f, 0.7071068f );
	additivePose.boneTransforms[1].scale = Vector3( 3.f, 3.f, 3.f );

	cmf::BlendAdditivePose( outPose, inputPose, basePose, additivePose, 1.0f );

	auto inverseBase = cmf::Inverse( basePose.boneTransforms[1] );
	auto diff = cmf::Multiply( inverseBase, additivePose.boneTransforms[1] );
	auto expected = cmf::Multiply( inputPose.boneTransforms[1], diff );
	EXPECT_EQ( outPose.boneTransforms[1].position, expected.position );
	EXPECT_EQ( outPose.boneTransforms[1].rotation, expected.rotation );
	EXPECT_EQ( outPose.boneTransforms[1].scale, expected.scale );
}

TEST( Animation, BlendAdditivePoseWithHalfAlpha )
{
	cmf::MemoryAllocator allocator;

	auto skeleton = MakeInPlaceSkeleton( {
											 { "Root", 0xffffffff, { { 0.f, 2.f, 0.f }, { 0.f, 0.3826834f, 0.f, 0.9238795f }, { 2.f, 2.f, 2.f } } },
											 { "Child", 0, { { 1.f, 0.5f, 0.f }, { 0.f, 0.f, 0.3826834f, 0.9238795f }, { 1.5f, 1.5f, 1.5f } } },
										 },
										 allocator );

	cmf::SkeletonPose basePose, additivePose, inputPose, outPose;
	cmf::RestPose( basePose, skeleton );
	cmf::RestPose( additivePose, skeleton );
	cmf::RestPose( inputPose, skeleton );

	// Make inputPose differ from basePose
	inputPose.boneTransforms[1].position = Vector3( 2.f, 3.f, 4.f );
	inputPose.boneTransforms[1].rotation = Quaternion( 0.f, 0.7071068f, 0.f, 0.7071068f );
	inputPose.boneTransforms[1].scale = Vector3( 2.f, 2.f, 2.f );

	additivePose.boneTransforms[1].position = Vector3( 5.f, 0.f, 0.f );
	additivePose.boneTransforms[1].rotation = Quaternion( 0.f, 0.7071068f, 0.f, 0.7071068f );
	additivePose.boneTransforms[1].scale = Vector3( 3.f, 3.f, 3.f );

	cmf::BlendAdditivePose( outPose, inputPose, basePose, additivePose, 0.5f );

	auto inverseBase = cmf::Inverse( basePose.boneTransforms[1] );
	auto diff = cmf::Multiply( inverseBase, additivePose.boneTransforms[1] );
	auto blended = cmf::Lerp( cmf::Transform{}, diff, 0.5f );
	auto expected = cmf::Multiply( inputPose.boneTransforms[1], blended );
	EXPECT_EQ( outPose.boneTransforms[1].position, expected.position );
	EXPECT_EQ( outPose.boneTransforms[1].rotation, expected.rotation );
	EXPECT_EQ( outPose.boneTransforms[1].scale, expected.scale );
}

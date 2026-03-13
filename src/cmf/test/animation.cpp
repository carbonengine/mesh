#include "gtest/gtest.h"

#include "cmf/animation.h"
#include "cmf/memallocator.h"


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
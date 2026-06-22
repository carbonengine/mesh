// Copyright © 2026 CCP ehf.

#include "gtest/gtest.h"
#include "cmf/transforms.h"

Vector3 RotatePoint( const Vector3& point, const Quaternion& rotation )
{
	return Vector3{
		rotation.w * point.x + rotation.y * point.z - rotation.z * point.y,
		rotation.w * point.y + rotation.z * point.x - rotation.x * point.z,
		rotation.w * point.z + rotation.x * point.y - rotation.y * point.x
	};
}

Vector3 RotatePoint2( const Vector3& point, const Quaternion& rotation )
{
	return Vector4( Conjugate( rotation ) * Quaternion( Vector4( point, 0.f ) ) * rotation ).GetXYZ();
}

TEST( Transforms, TransformPoint )
{
	cmf::Transform transform;
	transform.position = { 1.f, 2.f, 3.f };
	transform.rotation = Normalize( RotationQuaternion( Vector3( 1, 0, 0 ), 90.f / 180.f * 3.14159265f ) );
	transform.scale = { 2.f, 2.f, 2.f };
	Vector3 point = { 4.f, 5.f, 6.f };

	Vector3 transformedPoint = cmf::TransformPoint( point, transform );
	EXPECT_NEAR( transformedPoint.x, 1.f + 2.f * 4.f, 0.0001f );
	EXPECT_NEAR( transformedPoint.y, 2.f + 2.f * ( -6.f ), 0.0001f );
	EXPECT_NEAR( transformedPoint.z, 3.f + 2.f * 5.f, 0.0001f );
}

TEST( Transforms, Inverse )
{
	cmf::Transform transform;
	transform.position = { 1.f, 2.f, 3.f };
	transform.rotation = RotationQuaternion( Vector3( 1, 0, 0 ), 90.f / 180.f * 3.14159265f );
	transform.scale = { 2.f, 2.f, 2.f };
	cmf::Transform inverse = cmf::Inverse( transform );

	Vector3 point = { 4.f, 5.f, 6.f };
	auto transformed = cmf::TransformPoint( point, transform );
	auto roundTripped = cmf::TransformPoint( transformed, inverse );
	EXPECT_NEAR( point.x, roundTripped.x, 0.0001f );
	EXPECT_NEAR( point.y, roundTripped.y, 0.0001f );
	EXPECT_NEAR( point.z, roundTripped.z, 0.0001f );
}

TEST( Transforms, Multiply )
{
	cmf::Transform a;
	a.position = { 1.f, 2.f, 3.f };
	a.rotation = RotationQuaternion( Vector3( 1, 0, 0 ), 90.f / 180.f * 3.14159265f );
	a.scale = { 2.f, 2.f, 2.f };
	cmf::Transform b;
	b.position = { 4.f, 5.f, 6.f };
	b.rotation = RotationQuaternion( Vector3( 0, 1, 0 ), 90.f / 180.f * 3.14159265f );
	b.scale = { 0.5f, 0.5f, 0.5f };
	cmf::Transform c = cmf::Multiply( a, b );
	Vector3 point = { 7.f, 8.f, 9.f };
	auto transformedByA = cmf::TransformPoint( point, a );
	auto transformedByB = cmf::TransformPoint( transformedByA, b );
	auto transformedByC = cmf::TransformPoint( point, c );
	EXPECT_NEAR( transformedByB.x, transformedByC.x, 0.0001f );
	EXPECT_NEAR( transformedByB.y, transformedByC.y, 0.0001f );
	EXPECT_NEAR( transformedByB.z, transformedByC.z, 0.0001f );
}

TEST( Transforms, MultiplyNoScale )
{
	cmf::Transform a;
	a.position = { 1.f, 2.f, 3.f };
	a.rotation = RotationQuaternion( Vector3( 1, 0, 0 ), 90.f / 180.f * 3.14159265f );
	cmf::Transform b;
	b.position = { 4.f, 5.f, 6.f };
	b.rotation = RotationQuaternion( Vector3( 0, 1, 0 ), 90.f / 180.f * 3.14159265f );
	cmf::Transform c = cmf::Multiply( a, b );
	Vector3 point = { 7.f, 8.f, 9.f };
	auto transformedByA = cmf::TransformPoint( point, a );
	auto transformedByB = cmf::TransformPoint( transformedByA, b );
	auto transformedByC = cmf::TransformPoint( point, c );
	EXPECT_NEAR( transformedByB.x, transformedByC.x, 0.0001f );
	EXPECT_NEAR( transformedByB.y, transformedByC.y, 0.0001f );
	EXPECT_NEAR( transformedByB.z, transformedByC.z, 0.0001f );
}
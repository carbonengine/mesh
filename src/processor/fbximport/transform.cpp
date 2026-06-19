// Copyright © 2026 CCP ehf.

#include "transform.h"


Vector3 ToVector3( const ufbx_vec3& vec )
{
	return Vector3( float( vec.x ), float( vec.y ), float( vec.z ) );
}

Vector2 ToVector2( const ufbx_vec2& vec )
{
	return Vector2( float( vec.x ), float( vec.y ) );
}

Vector4 ToVector4( const ufbx_vec4& vec )
{
	return Vector4( float( vec.x ), float( vec.y ), float( vec.z ), float( vec.w ) );
}

Quaternion ToQuaternion( const ufbx_quat& vec )
{
	return Quaternion( float( vec.x ), float( vec.y ), float( vec.z ), float( vec.w ) );
}

Matrix ToMatrix( const ufbx_matrix& m )
{
	return Matrix(
		float( m.m00 ), float( m.m10 ), float( m.m20 ), 0, float( m.m01 ), float( m.m11 ), float( m.m21 ), 0, float( m.m02 ), float( m.m12 ), float( m.m22 ), 0, float( m.m03 ), float( m.m13 ), float( m.m23 ), 1 );
}

Matrix ToMatrix( const ufbx_transform& t )
{
	return ToMatrix( ufbx_transform_to_matrix( &t ) );
}

std::string ToString( ufbx_string str )
{
	return std::string( str.data, str.length );
}


CoordinateSystem::CoordinateSystem( float scale ) :
	m_scale( scale )
{
}

CoordinateSystem::CoordinateSystem( const ufbx_coordinate_axes& axes, float scale ) :
	m_scale( scale )
{
	auto ToVector = []( ufbx_coordinate_axis axis ) {
		switch( axis )
		{
		case UFBX_COORDINATE_AXIS_POSITIVE_X:
			return Vector3( 1, 0, 0 );
		case UFBX_COORDINATE_AXIS_POSITIVE_Y:
			return Vector3( 0, 1, 0 );
		case UFBX_COORDINATE_AXIS_POSITIVE_Z:
			return Vector3( 0, 0, 1 );
		case UFBX_COORDINATE_AXIS_NEGATIVE_X:
			return Vector3( -1, 0, 0 );
		case UFBX_COORDINATE_AXIS_NEGATIVE_Y:
			return Vector3( 0, -1, 0 );
		default:
			return Vector3( 0, 0, -1 );
		}
	};

	Vector3 up = ToVector( axes.up );
	Vector3 forward = ToVector( axes.front );
	Vector3 right = ToVector( axes.right );

	Matrix m;
	m.GetX() = right;
	m.GetY() = up;
	m.GetZ() = forward;
	m_transform = Transpose( m );
}

Vector3 CoordinateSystem::TransformPoint( const Vector3& v ) const
{
	return TransformVector( v ) * m_scale;
}

Vector3 CoordinateSystem::TransformVector( const Vector3& v ) const
{
	auto vv = TransformCoord( v, m_transform );
	return vv;
}

Quaternion CoordinateSystem::TransformRotation( const Quaternion& q ) const
{
	auto m = RotationMatrix( q );
	auto t = m_transform * m * Transpose( m_transform );
	Vector3 scale;
	Vector3 translation;
	Quaternion outQ;
	Decompose( scale, outQ, translation, t );
	return outQ;
}

Matrix CoordinateSystem::TransformMatrix( const Matrix& m ) const
{
	auto rot = m;
	rot.GetTranslation() = Vector3( 0, 0, 0 );
	rot = m_transform * rot * Transpose( m_transform );
	rot.GetTranslation() = TransformPoint( m.GetTranslation() );
	return rot;
}

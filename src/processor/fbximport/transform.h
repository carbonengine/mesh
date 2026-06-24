// Copyright © 2026 CCP ehf.

#pragma once

#include <CcpMath.h>
#include "ufbx.h"
#include <string>

Vector3 ToVector3( const ufbx_vec3& vec );
Vector2 ToVector2( const ufbx_vec2& vec );
Vector4 ToVector4( const ufbx_vec4& vec );
Quaternion ToQuaternion( const ufbx_quat& vec );
Matrix ToMatrix( const ufbx_matrix& m );
Matrix ToMatrix( const ufbx_transform& t );

std::string ToString( ufbx_string str );


/** 
* @brief Represents a coordinate system transformation from an FBX coordinate system to CMF one.
*/
struct CoordinateSystem
{
	explicit CoordinateSystem( float scale );
	CoordinateSystem( const ufbx_coordinate_axes& axes, float scale );

	/** @brief Transforms a point from the FBX coordinate system to the CMF coordinate system.
    * @param v The point to transform.
    * @return The transformed point.
    */
	Vector3 TransformPoint( const Vector3& v ) const;

	/** @brief Transforms a vector (direction) from the FBX coordinate system to the CMF coordinate system.
    * @param v The vector to transform.
    * @return The transformed vector.
    */
	Vector3 TransformVector( const Vector3& v ) const;

	/** @brief Transforms a rotation from the FBX coordinate system to the CMF coordinate system.
    * @param q The rotation to transform.
    * @return The transformed rotation.
    */
	Quaternion TransformRotation( const Quaternion& q ) const;

	/** @brief Transforms a matrix from the FBX coordinate system to the CMF coordinate system.
    * @param m The matrix to transform.
    * @return The transformed matrix.
    */
	Matrix TransformMatrix( const Matrix& m ) const;

	Matrix m_transform = IdentityMatrix();
	float m_scale = 1.f;
};

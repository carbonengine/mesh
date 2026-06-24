// Copyright © 2026 CCP ehf.

#pragma once

#include "cmf.h"

namespace cmf
{

/** @brief Interpolation between two transforms. 
* Performs linear interpolation for position and scale, and spherical linear interpolation for rotation.
* @param a The first transform.
* @param b The second transform.
* @param alpha The interpolation factor, between 0 and 1. 0 will return a, 1 will return b, and values in between will return a blend of the two.
*/
inline Transform Lerp( const Transform& a, const Transform& b, float alpha )
{
	return {
		a.position + ( b.position - a.position ) * alpha,
		Slerp( a.rotation, b.rotation, alpha ),
		a.scale + ( b.scale - a.scale ) * alpha
	};
}

/** @brief Computes the inverse of a transform.
* The inverse transform, when multiplied with the original, yields the identity transform. The input transform scale may not contain zero components,
* as this would lead to division by zero in the computation of the inverse scale.
* @param transform The transform to invert.
* @return The inverted transform.
*/
inline Transform Inverse( const Transform& transform )
{
	auto invRotation = XMQuaternionConjugate( transform.rotation );
	auto invScale = Vector3{ 1.f / transform.scale.x, 1.f / transform.scale.y, 1.f / transform.scale.z };
	return {
		XMVectorMultiply( XMVector3Rotate( XMVectorNegate( transform.position ), invRotation ), invScale ),
		invRotation,
		invScale
	};
}

/** @brief Converts a transform to a 4x4 matrix representation.
* @param transform The transform to convert.
* @return A matrix combining the scale, rotation, and position of the transform.
*/
inline Matrix ToMatrix( const Transform& transform )
{
	return TransformationMatrix( transform.scale, transform.rotation, transform.position );
}

/** @brief Multiplies two transforms together.
* When both transforms have unit scale, an optimized path using quaternion math is used.
* Otherwise, the transforms are converted to matrices, multiplied, and decomposed back.
* @param a The first (child/local) transform.
* @param b The second (parent/world) transform.
* @return The combined transform.
*/
inline Transform Multiply( const Transform& a, const Transform& b )
{
	if( a.scale == Vector3( 1, 1, 1 ) && b.scale == Vector3( 1, 1, 1 ) )
	{
		return {
			XMVectorAdd( XMVector3Rotate( a.position, b.rotation ), b.position ),
			XMQuaternionMultiply( a.rotation, b.rotation ),
			Vector3( 1, 1, 1 )
		};
	}
	auto am = ToMatrix( a );
	auto bm = ToMatrix( b );
	auto combined = am * bm;
	Transform result = {};
	Decompose( result.scale, result.rotation, result.position, combined );
	return result;
}

/** @brief Transforms a point by applying scale, rotation, and translation from the given transform.
* @param point The point to transform.
* @param transform The transform to apply.
* @return The transformed point.
*/
inline Vector3 TransformPoint( const Vector3& point, const Transform& transform )
{
	return XMVectorAdd( XMVector3Rotate( XMVectorMultiply( point, transform.scale ), transform.rotation ), transform.position );
}

}
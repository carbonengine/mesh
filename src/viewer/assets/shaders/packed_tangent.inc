// Copyright (c) 2026 CCP ehf.

struct TangentSpace {
	vec3 tangent;
	vec3 binormal;
	vec3 normal;
};

vec2 SinCosFloat( float angle ) 
{
    return vec2( sin( angle ), cos( angle ) );
}

float RemapTangentAngle( float angle )
{
	return clamp( ( angle + 3.14159265359 ) / 6.28318530718, 0.0f, 1.0f );
}

vec4 PackTangentSpaceLegacy( TangentSpace space )
{
	vec4 angles;
	angles.x = atan( space.tangent.y, space.tangent.x );
	angles.y = acos( clamp( space.tangent.z, -1.0f, 1.0f ) );
	angles.z = atan( space.binormal.y, space.binormal.x );
	angles.w = acos( clamp( space.binormal.z, -1.0f, 1.0f ) );

	if( dot( space.normal, cross( space.tangent, space.binormal ) ) < 0.0f )
	{
		angles.y = -angles.y;
		angles.w = -angles.w;
	}

	return vec4(
		RemapTangentAngle( angles.x ),
		RemapTangentAngle( angles.y ),
		RemapTangentAngle( angles.z ),
		RemapTangentAngle( angles.w ) );
}

TangentSpace UnpackTangentSpaceLegacy( vec4 tangents )
{
	vec4 angles = tangents * 6.28318530718 - 3.14159265359;
	vec4 sc0, sc1;
	sc0.xy = SinCosFloat( angles.x );
	sc0.zw = SinCosFloat( angles.y );
	sc1.xy = SinCosFloat( angles.z );
	sc1.zw = SinCosFloat( angles.w );

    TangentSpace space;

	vec3 tangent = vec3( sc0.y * abs( sc0.z ), sc0.x * abs( sc0.z ), sc0.w );
	vec3 binormal = vec3( sc1.y * abs( sc1.z ), sc1.x * abs( sc1.z ), sc1.w );
	vec3 normal = -cross( tangent, binormal );
    if( angles.y > 0.0 && angles.w > 0.0 ) normal = -normal;

    return TangentSpace( tangent, binormal, normal );
}

vec4 PackTangentSpace( TangentSpace space )
{
	float normalSign = dot( space.normal, cross( space.tangent, space.binormal ) ) < 0.0f ? -1.0f : 1.0f;
	vec3 normal = space.normal * normalSign;

	float m00 = space.tangent.x;
	float m01 = space.tangent.y;
	float m02 = space.tangent.z;
	float m10 = space.binormal.x;
	float m11 = space.binormal.y;
	float m12 = space.binormal.z;
	float m20 = normal.x;
	float m21 = normal.y;
	float m22 = normal.z;

	vec4 q;
	float trace = m00 + m11 + m22;
	if( trace > 0.0f )
	{
		float s = sqrt( trace + 1.0f ) * 2.0f;
		q.x = ( m12 - m21 ) / s;
		q.y = ( m20 - m02 ) / s;
		q.z = ( m01 - m10 ) / s;
		q.w = 0.25f * s;
	}
	else if( m00 > m11 && m00 > m22 )
	{
		float s = sqrt( 1.0f + m00 - m11 - m22 ) * 2.0f;
		q.x = 0.25f * s;
		q.y = ( m01 + m10 ) / s;
		q.z = ( m20 + m02 ) / s;
		q.w = ( m12 - m21 ) / s;
	}
	else if( m11 > m22 )
	{
		float s = sqrt( 1.0f + m11 - m00 - m22 ) * 2.0f;
		q.x = ( m01 + m10 ) / s;
		q.y = 0.25f * s;
		q.z = ( m12 + m21 ) / s;
		q.w = ( m20 - m02 ) / s;
	}
	else
	{
		float s = sqrt( 1.0f + m22 - m00 - m11 ) * 2.0f;
		q.x = ( m20 + m02 ) / s;
		q.y = ( m12 + m21 ) / s;
		q.z = 0.25f * s;
		q.w = ( m01 - m10 ) / s;
	}

	q = normalize( q );
	if( q.w < 0.0f )
	{
		q = -q;
	}

	return vec4( q.xyz, normalSign );
}

TangentSpace UnpackTangentSpace( vec4 t ) {
	// Heavily optimized shader code that constructs the TBN matrix.

	// Extract the xyz components and square them
	float x = t.x;
	float y = t.y;
	float z = t.z;
	float x2 = x * x;
	float y2 = y * y;
	float z2 = z * z;

	// Optimized fma() chain to reconstruct W = sqrt(1 - x2 - y2 - z2)
	// Don't use the above x2, y2 and z2 values, to reduce pipeline dependencies.
	float w2 = clamp( fma( z, -z, fma( y, -y, fma( x, -x, 1.0f ) ) ), 0.0f, 1.0f );
	float w = sqrt( w2 );

	// Calculate shared values.
	// These multiplications by 2.0f are free on some GPUs.
	float xy = x * y * 2.0f;
	float xz = x * z * 2.0f;
	float yz = y * z * 2.0f;
	float xw = x * w * 2.0f;
	float yw = y * w * 2.0f;
	float zw = z * w * 2.0f;

	// Compute the three vectors. 
	TangentSpace space;

	space.tangent = vec3( fma( -2.0f, y2, fma( -2.0f, z2, 1.0f ) ), + xy + zw, + xz - yw );
	space.binormal = vec3( - zw + xy, fma( -2.0f, x2, fma( -2.0f, z2, 1.0f ) ), + yz + xw );
	space.normal = vec3( + yw + xz, + yz - xw, fma( -2.0f, x2, fma( -2.0f, y2, 1.0f ) ) ) * t.w; // packed normal sign multiplication
	
	return space;
}

TangentSpace Orthonormalize( TangentSpace space )
{
    float handedness = dot( space.normal, cross( space.tangent, space.binormal ) ) < 0.0f ? -1.0f : 1.0f;

	space.normal = normalize( space.normal );
	space.tangent = normalize( space.tangent - space.normal * dot( space.normal, space.tangent ) );
	space.binormal = cross( space.normal, space.tangent ) * handedness;

    return space;
}
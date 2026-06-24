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
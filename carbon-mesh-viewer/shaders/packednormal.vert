#version 450

// Constants
layout( binding = 0 ) uniform PerFrame 
{
    mat4 projectionMatrix;
    mat4 viewMatrix;
} perframe;


// Inputs
layout( location = 0 ) in vec3 inPosition;
layout( location = 32 ) in vec4 inPackedTangents;

// Outputs
layout( location = 0 ) out vec3 viewPosition;
layout( location = 1 ) out vec3 normal;


struct TangentSpace {
	vec3 tangent;
	vec3 bitangent;
	vec3 normal;
};

TangentSpace unpackTangentSpace( vec4 t ) {
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
	space.bitangent = vec3( - zw + xy, fma( -2.0f, x2, fma( -2.0f, z2, 1.0f ) ), + yz + xw );
	space.normal = vec3( + yw + xz, + yz - xw, fma( -2.0f, x2, fma( -2.0f, y2, 1.0f ) ) ) * t.w; // packed normal sign multiplication
	
	return space;
}

void main() 
{
    viewPosition = ( perframe.viewMatrix * vec4( inPosition, 1.0 ) ).xyz;
    gl_Position = perframe.projectionMatrix * vec4( viewPosition, 1.0 );
	
	TangentSpace space = unpackTangentSpace( inPackedTangents );
	normal = ( perframe.viewMatrix * vec4( space.normal, 0.0 ) ).xyz;
}
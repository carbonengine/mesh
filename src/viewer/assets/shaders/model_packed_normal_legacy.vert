// Copyright (c) 2025 CCP ehf.

#version 450

// Constants
layout( binding = 0 ) uniform PerFrame 
{
    mat4 projectionMatrix;
    mat4 viewMatrix;
} perframe;


// Inputs
layout( location = 0 ) in vec3 inPosition;
layout( location = 1 ) in vec4 inPackedTangentsLegacy;

// Outputs
layout( location = 0 ) out vec3 viewPosition;
layout( location = 1 ) out vec3 normal;


struct TangentSpace {
	vec3 tangent;
	vec3 bitangent;
	vec3 normal;
};

vec2 SinCosFloat( float angle ) 
{
    return vec2( sin( angle ), cos( angle ) );
}

TangentSpace unpackTangentSpace( vec4 tangents )
{
	vec4 angles = tangents * 6.28318530718 - 3.14159265359;
	vec4 sc0, sc1;
	sc0.xy = SinCosFloat( angles.x );
	sc0.zw = SinCosFloat( angles.y );
	sc1.xy = SinCosFloat( angles.z );
	sc1.zw = SinCosFloat( angles.w );

    TangentSpace space;

	vec3 tangent = vec3( sc0.y * abs( sc0.z ), sc0.x * abs( sc0.z ), sc0.w );
	vec3 bitangent = vec3( sc1.y * abs( sc1.z ), sc1.x * abs( sc1.z ), sc1.w );
	vec3 normal = -cross( tangent, bitangent );
	//normal = all( angles.yw > 0.0 ) ? -normal : normal;
    if( angles.y > 0.0 && angles.w > 0.0 ) normal = -normal;

    return TangentSpace( tangent, bitangent, normal );
}

void main() 
{
    viewPosition = ( perframe.viewMatrix * vec4( inPosition, 1.0 ) ).xyz;
    gl_Position = perframe.projectionMatrix * vec4( viewPosition, 1.0 );
	
	TangentSpace space = unpackTangentSpace( inPackedTangentsLegacy );
	normal = ( perframe.viewMatrix * vec4( space.normal, 0.0 ) ).xyz;
}
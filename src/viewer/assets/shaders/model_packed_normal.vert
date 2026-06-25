// Copyright (c) 2025 CCP ehf.

#version 450
#pragma multi_usage inPackedTangents

#include "packed_tangent.inc"

// Constants
layout( binding = 0 ) uniform PerFrame 
{
    mat4 projectionMatrix;
    mat4 viewMatrix;
} perframe;


// Inputs
layout( location = 0 ) in vec3 inPosition;
layout( location = 1 ) in vec4 inPackedTangents;

// Outputs
layout( location = 0 ) out vec3 viewPosition;
layout( location = 1 ) out vec3 normal;

void main() 
{
    viewPosition = ( perframe.viewMatrix * vec4( inPosition, 1.0 ) ).xyz;
    gl_Position = perframe.projectionMatrix * vec4( viewPosition, 1.0 );
	
	TangentSpace space = UnpackTangentSpace( inPackedTangents );
	normal = ( perframe.viewMatrix * vec4( space.normal, 0.0 ) ).xyz;
}
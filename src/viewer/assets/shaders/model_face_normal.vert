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

// Outputs
layout( location = 0 ) out vec3 viewPosition;


void main() 
{
    viewPosition = ( perframe.viewMatrix * vec4( inPosition, 1.0 ) ).xyz;
    gl_Position = perframe.projectionMatrix * vec4( viewPosition, 1.0 );
}
// Copyright (c) 2026 CCP ehf.

#version 450
#pragma multi_usage inColor

// Constants
layout( binding = 0 ) uniform PerFrame 
{
    mat4 projectionMatrix;
    mat4 viewMatrix;
} perframe;


// Inputs
layout( location = 0 ) in vec3 inPosition;
layout( location = 1 ) in vec3 inColor; 

// Outputs;
layout( location = 0 ) out vec3 color;


void main() 
{
    color = inColor;
    gl_Position = perframe.projectionMatrix * perframe.viewMatrix * vec4( inPosition, 1.0 );
}
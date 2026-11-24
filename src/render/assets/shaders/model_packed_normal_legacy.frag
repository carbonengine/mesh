// Copyright (c) 2025 CCP ehf.

#version 450

// Inputs
layout( location = 0 ) in vec3 viewPosition;
layout( location = 1 ) in vec3 normal;

// Outputs
layout( location = 0 ) out vec4 outFragColor;

void main() 
{
    vec3 viewDirection = -normalize( viewPosition );
    float lighting = clamp( dot( normal, viewDirection ), 0.0, 1.0 );
    outFragColor = vec4( vec3( lighting ), 1.0 );
}
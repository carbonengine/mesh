// Copyright (c) 2026 CCP ehf.

#version 450

// Inputs
layout( location = 0 ) in vec3 color;

// Outputs
layout( location = 0 ) out vec4 outFragColor;

void main() 
{
    outFragColor = vec4( color, 1.0 );
}
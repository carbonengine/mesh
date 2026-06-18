// Copyright (c) 2026 CCP ehf.

#version 450

// Inputs
layout( location = 0 ) in vec3 vertColor;

// Outputs
layout( location = 0 ) out vec4 outFragColor;

void main() 
{
    outFragColor = vec4( vertColor, 1.0 );
}
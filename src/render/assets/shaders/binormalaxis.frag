// Copyright (c) 2026 CCP ehf.

#version 450

// inputs
layout( location = 0 ) in vec3 inFragColor;

// Outputs
layout( location = 0 ) out vec4 outFragColor;

void main()
{
    outFragColor = vec4( inFragColor, 1.0 );
}
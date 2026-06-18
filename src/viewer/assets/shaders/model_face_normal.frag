// Copyright (c) 2025 CCP ehf.

#version 450

// Inputs
layout( location = 0 ) in vec3 viewPosition;

// Outputs
layout( location = 0 ) out vec4 outFragColor;

void main() 
{
    vec3 viewDirection = -normalize( viewPosition );
    vec3 faceNormal = normalize( cross( dFdy( viewPosition ), dFdx( viewPosition ) ) );
    float lighting = clamp( dot( faceNormal, viewDirection ), 0.0, 1.0 );

    outFragColor = vec4( vec3( lighting ), 1.0 );
}
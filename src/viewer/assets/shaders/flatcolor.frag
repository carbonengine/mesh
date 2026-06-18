// Copyright (c) 2026 CCP ehf.

#version 450
// Inputs
layout( location = 0 ) in vec3 viewPosition;
layout( location = 1 ) in vec4 vertexColor;

// Outputs
layout( location = 0 ) out vec4 outFragColor;

void main() 
{   	
    vec3 viewDirection = -normalize( viewPosition );
    vec3 faceNormal = normalize( cross( dFdy( viewPosition ), dFdx( viewPosition ) ) );
    vec3 colorModifier = vec3( clamp( dot( faceNormal, viewDirection ), 0.0, 1.0 ) );
	// conditionally use the colorModifier, but don't use if, because that will screw up the dFdy and dFdx calls
    outFragColor = vec4( vertexColor.w * vertexColor.rgb * colorModifier + (1.0 - vertexColor.w) * vertexColor.rgb, 1.0 );
}
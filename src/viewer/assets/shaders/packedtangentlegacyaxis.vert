// Copyright (c) 2026 CCP ehf.

#version 450
#pragma multi_usage inPackedTangentsLegacy

#include "packed_tangent.inc"

// Constants
layout( binding = 0 ) uniform PerFrame 
{
    mat4 projectionMatrix;
    mat4 viewMatrix;
} perframe;

layout( binding = 1 ) uniform AxisConfig
{
	vec3 color;
	float scale;
	int axisSelector;
} axisConfig;

// Inputs
// inputs are per instance, not per vertex
layout( location = 0 ) in vec3 inPosition;
layout( location = 1 ) in vec4 inPackedTangentsLegacy;  

// output 
layout ( location = 0 ) out vec3 outColor;


void main() 
{
	vec4 position = vec4( inPosition, 1.0 );
	if( gl_VertexIndex == 1 )
	{
		TangentSpace space = UnpackTangentSpaceLegacy( inPackedTangentsLegacy );
		vec3 axis = vec3(0.0);
		if( axisConfig.axisSelector == 0 )
		{
			axis = space.normal;
		}
		else if( axisConfig.axisSelector == 1 )
		{
			axis = space.tangent;
		}
		else if( axisConfig.axisSelector == 2 )
		{
			axis = space.binormal;
		}	
		position.xyz += axisConfig.scale * axis;
	}

    gl_Position = perframe.projectionMatrix * perframe.viewMatrix * position;

	outColor = axisConfig.color;
}
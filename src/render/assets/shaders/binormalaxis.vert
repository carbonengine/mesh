// Copyright (c) 2026 CCP ehf.

#version 450
#pragma multi_usage inBinormal

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
layout( location = 1 ) in vec3 inBinormal;

// Output
layout ( location = 0 ) out vec3 outColor;

void main() 
{
	vec4 position = vec4( inPosition, 1.0 );

	if( gl_VertexIndex == 1 )
	{
		position.xyz += axisConfig.scale * inBinormal;
	}

    gl_Position = perframe.projectionMatrix * perframe.viewMatrix * position;
	outColor = axisConfig.color;
}
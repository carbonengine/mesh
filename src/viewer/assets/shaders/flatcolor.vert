// Copyright (c) 2026 CCP ehf.

#version 450

layout( binding = 0 ) uniform UBO 
{
    mat4 projectionMatrix;
    mat4 viewMatrix;
    mat4 modelMatrix;
    vec4 boneInfo; // x = boneCount, y = how many bones transforms are used per instance
} ubo;

readonly layout( std430, binding = 1 ) buffer BoneTransforms {
	mat4x4 boneTransforms[ ];
};

readonly layout( std430, binding = 2 ) buffer VertexToBoneIndex {
	uint vertexIndexToBoneIndex[ ];
};

readonly layout( std430, binding = 3 ) buffer SelectedBones {
	uint selectedBoneIndices[ ];
};

layout( binding = 4 ) uniform ColorInfo 
{
    vec4 unselected; // rgb is color, a is style 0 = flat, 1 = face normal
	vec4 selected; // rgb is color, a is style 0 = flat, 1 = face normal
} color;

// inputs
layout( location = 0 ) in vec4 inPosition;

// Outputs
layout( location = 0 ) out vec3 viewPosition;
layout( location = 1 ) out vec4 vertexColor;

void main() 
{  
    mat4x4 combinedMatrix = ubo.viewMatrix;
	
    vertexColor = color.unselected.rgba;

    if( ubo.boneInfo.x > 0 )
    {
		uint vertexIndex = uint( inPosition.w ) + gl_InstanceIndex * uint( ubo.boneInfo.y );
		uint index = vertexIndexToBoneIndex[ vertexIndex ];

        combinedMatrix = combinedMatrix * boneTransforms[ index ];
		if( selectedBoneIndices[ index ] == 1 )
		{
			vertexColor = color.selected.rgba;
		}
    }
    combinedMatrix = combinedMatrix * ubo.modelMatrix;
    vec4 pos = combinedMatrix * vec4( inPosition.xyz, 1.0 );
    viewPosition = pos.xyz;

	gl_Position = ubo.projectionMatrix * pos;

}
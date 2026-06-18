// Copyright (c) 2025 CCP ehf.

#version 450
#extension GL_KHR_vulkan_glsl: enable

// Constants
layout( binding = 0 ) uniform UBO 
{
    mat4 projectionMatrix;
    mat4 viewMatrix;
    mat4 modelMatrix;
    uint boneCount;
} ubo;

readonly layout(std430, binding = 1) buffer BoneTransforms {
	mat4x4 boneTransforms[ ];
};

// Inputs
layout( location = 0 ) in vec3 inPosition;
layout( location = 1 ) in vec3 inColor;

// Outputs
layout( location = 0 ) out vec3 fragColor;

void main() 
{
    mat4x4 combinedMatrix = ubo.projectionMatrix * ubo.viewMatrix;
    if( ubo.boneCount > 0 )
    {
        combinedMatrix = combinedMatrix * boneTransforms[gl_InstanceIndex];
    }
    combinedMatrix = combinedMatrix * ubo.modelMatrix;

    gl_Position = combinedMatrix * vec4( inPosition, 1.0 );
    fragColor = inColor;
}
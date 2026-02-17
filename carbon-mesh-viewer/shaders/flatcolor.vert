#version 450

layout( binding = 0 ) uniform UBO 
{
    mat4 projectionMatrix;
    mat4 viewMatrix;
    mat4 modelMatrix;
} ubo;


layout( location = 0 ) in vec3 inPosition;


void main() 
{
    vec3 viewPosition = ( ubo.viewMatrix * ubo.modelMatrix * vec4( inPosition, 1.0 ) ).xyz;
    gl_Position = ubo.projectionMatrix * vec4( viewPosition, 1.0 );
}
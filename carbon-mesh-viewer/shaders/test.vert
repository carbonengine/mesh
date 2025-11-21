#version 450

layout( location = 0 ) in vec3 inPos;

layout( binding = 0 ) uniform PerFrame 
{
    mat4 projectionMatrix;
    mat4 viewMatrix;
} perframe;

layout( location = 0 ) out vec3 viewPosition;

void main() 
{
    viewPosition = ( perframe.viewMatrix * vec4( inPos, 1.0 ) ).xyz;
    gl_Position = perframe.projectionMatrix * vec4( viewPosition, 1.0 );
}
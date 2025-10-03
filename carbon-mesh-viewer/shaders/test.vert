#version 450

layout (location = 0) in vec3 inPos;

layout (binding = 0) uniform PerFrame 
{
    mat4 projectionMatrix;
    mat4 viewMatrix;
} perframe;

out gl_PerVertex 
{
    vec4 gl_Position;   
};

void main() 
{
    gl_Position = perframe.projectionMatrix * perframe.viewMatrix * vec4(inPos.xyz, 1.0);
}
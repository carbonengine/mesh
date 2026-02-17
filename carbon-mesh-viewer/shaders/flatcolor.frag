
#version 450
layout( binding = 1 ) uniform UBO 
{
    vec3 color;
} ubo;

layout( location = 0 ) out vec4 outFragColor;

void main() 
{
    outFragColor = vec4( ubo.color, 1.0 );
}
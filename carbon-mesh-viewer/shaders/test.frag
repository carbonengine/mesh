
#version 450
layout (location = 0) out vec4 outFragColor;

layout(location = 0) in vec3 viewPosition;

void main() 
{
   
#define FILL
#ifdef FILL
    //Note: this will not work with wireframe/line rendering, as the gradients will not make sense! Will produce black/NaN in that case.
    //Use a different shader in that case.
    vec3 faceNormal = normalize( cross( dFdx( viewPosition ), dFdy( viewPosition ) ) );
    vec3 viewDirection = normalize(viewPosition);
    float lighting = clamp( dot( faceNormal, viewDirection ), 0.0, 1.0 );
#else
    //This needs to be hooked up.
    float lighting = 1.0;
#endif


    outFragColor = vec4( vec3( lighting ), 1.0 );
}
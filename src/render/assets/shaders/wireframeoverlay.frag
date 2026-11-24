// Copyright (c) 2026 CCP ehf.

#version 450
#extension GL_EXT_fragment_shader_barycentric : require
// parts of the source obtained from:
// https://github.com/nvpro-samples/vk_mini_samples/tree/main/samples/barycentric_wireframe


layout(location = 0) out vec4 outColor;

// Return the width [0..1] for which the line should be displayed or not
float getLineWidth(in vec3 deltas, in float thickness, in float smoothing, in vec3 barys)
{
  barys = smoothstep(deltas * (thickness), deltas * (thickness + smoothing), barys);
  float minBary = min(barys.x, min(barys.y, barys.z));
  return 1.0 - minBary;
}

// Position along the edge [0..1]
float edgePosition()
{
  return max(gl_BaryCoordEXT.z, max(gl_BaryCoordEXT.y, gl_BaryCoordEXT.x));
}

// Return 0 or 1 if edgePos should be diplayed or not
float stipple(in float stippleRepeats, in float stippleLength, in float edgePos)
{
  float offset = 1.0 / stippleRepeats;
  offset *= 0.5 * stippleLength;
  float pattern = fract((edgePos + offset) * stippleRepeats);
  return 1.0 - step(stippleLength, pattern);
}

void main()
{
  float thickness = 0.15;  
  vec3  wireColor = vec3(0, 0, 0);            
  float smoothing = 0.70;  

  // Uniform position on the edge [0, 1]
  float edgePos = edgePosition();

  vec3 deltas = fwidthFine(gl_BaryCoordEXT);
  float lineWidth = getLineWidth(deltas, thickness, smoothing, gl_BaryCoordEXT);

  if(!gl_FrontFacing)
  {
    // stipple the back facing edges
    wireColor = vec3(0.3, 0.3, 0.3);
    float stippleFact = stipple(1.5, 1.0/3.0, edgePos);
    lineWidth *= stippleFact;  // 0 or 1
  }
  
  outColor = vec4(wireColor, lineWidth);

}
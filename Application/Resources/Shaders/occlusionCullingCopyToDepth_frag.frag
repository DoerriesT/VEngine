#version 450

#include "occlusionCullingCopyToDepth_bindings.h"

layout(set = INPUT_IMAGE_SET, binding = INPUT_IMAGE_BINDING) uniform usampler2D uReprojectedDepthBuffer;

layout(location = 0) in vec2 vTexCoords;

void main() 
{
	uint depthU = texelFetch(uReprojectedDepthBuffer, ivec2(gl_FragCoord.xy), 0).x;
	
	gl_FragDepth = 1.0 - uintBitsToFloat(depthU);
}


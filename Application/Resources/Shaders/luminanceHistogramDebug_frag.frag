#version 450

#include "luminanceHistogramDebug_bindings.h"

layout(set = LUMINANCE_HISTOGRAM_SET, binding = LUMINANCE_HISTOGRAM_BINDING) buffer LUMINANCE_HISTOGRAM
{
    uint uLuminanceHistogram[];
};

layout(push_constant) uniform PUSH_CONSTS 
{
	PushConsts uPushConsts;
};

layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 oColor;

void main() 
{
	uint bucket = uint(vTexCoords.x * LUMINANCE_HISTOGRAM_SIZE);
	uint value = uLuminanceHistogram[bucket];
	float cutoff = (float(value) * uPushConsts.invPixelCount) * 2.0;
	
	if (1.0 - vTexCoords.y > cutoff)
	{
		discard;
	}
	
	oColor = vec4(1.0, 0.0, 0.0, 1.0);
}


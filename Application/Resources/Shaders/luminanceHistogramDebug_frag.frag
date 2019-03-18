#version 450

#include "common.h"

layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 oColor;

void main() 
{
	uint bucket = uint(vTexCoords.x * LUMINANCE_HISTOGRAM_SIZE);
	uint value = uLuminanceHistogram.data[bucket];
	float cutoff = (float(value) / (uPerFrameData.width * uPerFrameData.height)) * 2.0;
	
	if (1.0 - vTexCoords.y > cutoff)
	{
		discard;
	}
	
	oColor = vec4(1.0, 0.0, 0.0, 1.0);
}


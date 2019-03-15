#version 450

#include "common.h"

layout(location = 0) in vec3 vColor;
layout(location = 1) in vec2 vTexCoords;
layout(location = 2) in flat uint vTextureIndex;

layout(location = 0) out vec4 oColor;

void main() 
{
	oColor = vec4(vColor, textureLod(uTextures[vTextureIndex - 1], vTexCoords, 0).x);
}


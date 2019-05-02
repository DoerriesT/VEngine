#version 450

#include "shadows_bindings.h"

layout(set = TEXTURES_SET, binding = TEXTURES_BINDING) uniform sampler2D uTextures[TEXTURE_ARRAY_SIZE];

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) flat in uint vAlbedoTextureIndex;

void main() 
{
	float alpha = 1.0;
	if (vAlbedoTextureIndex != 0)
	{
		alpha = texture(uTextures[vAlbedoTextureIndex - 1], vTexCoord).a;
		alpha *= 1.0 + textureQueryLod(uTextures[vAlbedoTextureIndex - 1], vTexCoord).x * ALPHA_MIP_SCALE;
		if(alpha < ALPHA_CUTOFF)
		{
			discard;
		}
	}
}


#version 450

#extension GL_EXT_nonuniform_qualifier : enable

#include "shadows_bindings.h"

layout(set = TEXTURES_SET, binding = TEXTURES_BINDING) uniform texture2D uTextures[TEXTURE_ARRAY_SIZE];
layout(set = SAMPLERS_SET, binding = SAMPLERS_BINDING) uniform sampler uSamplers[SAMPLER_COUNT];

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) flat in uint vAlbedoTextureIndex;

void main() 
{
	float alpha = 1.0;
	if (vAlbedoTextureIndex != 0)
	{
		alpha = texture(sampler2D(uTextures[nonuniformEXT(vAlbedoTextureIndex - 1)], uSamplers[SAMPLER_LINEAR_REPEAT]), vTexCoord).a;
		alpha *= 1.0 + textureQueryLod(sampler2D(uTextures[nonuniformEXT(vAlbedoTextureIndex - 1)], uSamplers[SAMPLER_LINEAR_REPEAT]), vTexCoord).x * ALPHA_MIP_SCALE;
		if(alpha < ALPHA_CUTOFF)
		{
			discard;
		}
	}
}


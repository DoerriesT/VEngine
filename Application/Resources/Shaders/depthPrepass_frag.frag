#version 450

#extension GL_EXT_nonuniform_qualifier : enable

#include "depthPrepass_bindings.h"
#include "common.h"

layout(set = MATERIAL_DATA_SET, binding = MATERIAL_DATA_BINDING) readonly buffer MATERIAL_DATA 
{
    MaterialData uMaterialData[];
};

layout(set = TEXTURES_SET, binding = TEXTURES_BINDING) uniform texture2D uTextures[TEXTURE_ARRAY_SIZE];
layout(set = SAMPLERS_SET, binding = SAMPLERS_BINDING) uniform sampler uSamplers[SAMPLER_COUNT];


layout(location = 0) in vec2 vTexCoord;
layout(location = 1) flat in uint vMaterialIndex;

void main() 
{
	uint albedoTextureIndex = (uMaterialData[vMaterialIndex].albedoNormalTexture & 0xFFFF0000) >> 16;
	bool texturePresent = albedoTextureIndex != 0;
	albedoTextureIndex = texturePresent ? albedoTextureIndex - 1 : albedoTextureIndex;
	float alpha = texture(sampler2D(uTextures[nonuniformEXT(albedoTextureIndex)], uSamplers[SAMPLER_LINEAR_REPEAT]), vTexCoord).a;
	alpha *= 1.0 + textureQueryLod(sampler2D(uTextures[nonuniformEXT(albedoTextureIndex)], uSamplers[SAMPLER_LINEAR_REPEAT]), vTexCoord).x * ALPHA_MIP_SCALE;
	if (texturePresent && alpha < ALPHA_CUTOFF)
	{
		discard;
	}
}


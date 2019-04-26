#version 450

#include "shadows_bindings.h"

layout(set = MATERIAL_DATA_SET, binding = MATERIAL_DATA_BINDING) readonly buffer MATERIAL_DATA 
{
    MaterialData uMaterialData[];
};

layout(set = TEXTURES_SET, binding = TEXTURES_BINDING) uniform sampler2D uTextures[TEXTURE_ARRAY_SIZE];

layout(push_constant) uniform PUSH_CONSTS 
{
	PushConsts uPushConsts;
};

layout(location = 0) in vec2 vTexCoord;

void main() 
{
	MaterialData materialData = uMaterialData[uPushConsts.materialIndex];
	
	float alpha = 1.0;
	uint albedoTextureIndex = (materialData.albedoNormalTexture & 0xFFFF0000) >> 16;
	if (albedoTextureIndex != 0)
	{
		alpha = texture(uTextures[albedoTextureIndex - 1], vTexCoord).a;
		alpha *= 1.0 + textureQueryLod(uTextures[albedoTextureIndex - 1], vTexCoord).x * ALPHA_MIP_SCALE;
		if(alpha < ALPHA_CUTOFF)
		{
			discard;
		}
	}
}


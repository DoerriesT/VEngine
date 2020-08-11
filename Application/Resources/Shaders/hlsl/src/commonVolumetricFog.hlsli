#ifndef COMMON_VOLUMETRIC_FOG_H
#define COMMON_VOLUMETRIC_FOG_H

#include "common.hlsli"
//#include "commonNoise.hlsli"

float volumetricFogGetDensity(GlobalParticipatingMedium medium, float3 position, Texture3D tex[TEXTURE_ARRAY_SIZE], SamplerState linearSampler)
{
	float density = position.y > medium.maxHeight ? 0.0 : 1.0;
	if (medium.heightFogEnabled != 0 && position.y > medium.heightFogStart)
	{
		float h = position.y - medium.heightFogStart;
		density *= exp(-h * medium.heightFogFalloff);
	}
	if (medium.densityTexture != 0)
	{
		float3 uv = frac(position * medium.textureScale + medium.textureBias);
		float noise = tex[medium.densityTexture - 1].SampleLevel(linearSampler, uv, 0.0).x;
		//float noise = saturate(perlinNoise() * 0.5 + 0.5) * saturate(medium.noiseIntensity);
		density *= noise;
	}
	return density;
}

float volumetricFogGetDensity(LocalParticipatingMedium medium, float3 position, Texture3D tex[TEXTURE_ARRAY_SIZE], SamplerState linearSampler)
{
	position = position * 0.5 + 0.5;
	float density = 1.0;
	if (medium.heightFogStart < 1.0 && position.y > medium.heightFogStart)
	{
		float h = position.y - medium.heightFogStart;
		density *= exp(-h * medium.heightFogFalloff);
	}
	if (medium.densityTexture != 0)
	{
		float3 uv = frac(position * medium.textureScale + medium.textureBias);
		float noise = tex[medium.densityTexture - 1].SampleLevel(linearSampler, uv, 0.0).x;
		//float noise = saturate(perlinNoise() * 0.5 + 0.5) * saturate(medium.noiseIntensity);
		density *= noise;
	}
	return density;
}

#endif // COMMON_VOLUMETRIC_FOG_H
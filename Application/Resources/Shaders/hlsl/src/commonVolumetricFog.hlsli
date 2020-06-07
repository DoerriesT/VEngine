#ifndef COMMON_VOLUMETRIC_FOG_H
#define COMMON_VOLUMETRIC_FOG_H

#include "common.hlsli"
#include "commonNoise.hlsli"

float volumetricFogGetDensity(GlobalParticipatingMedium medium, float3 position)
{
	float density = position.y > medium.maxHeight ? 0.0 : 1.0;
	if (medium.heightFogEnabled != 0 && position.y > medium.heightFogStart)
	{
		float h = position.y - medium.heightFogStart;
		density *= exp(-h * medium.heightFogFalloff);
	}
	if (medium.noiseIntensity > 0.0)
	{
		float noise = saturate(perlinNoise(position * medium.noiseScale + medium.noiseBias) * 0.5 + 0.5) * saturate(medium.noiseIntensity);
		density *= lerp(1.0, noise * noise, medium.noiseIntensity);
	}
	return density;
}

float volumetricFogGetDensity(LocalParticipatingMedium medium, float3 position)
{
	float density = 1.0;
	if (medium.noiseIntensity != 0)
	{
		float noise = saturate(perlinNoise((position * 0.5 + 0.5) * medium.noiseScale + medium.noiseBias) * 0.5 + 0.5) * saturate(medium.noiseIntensity);
		density *= lerp(1.0, noise * noise, medium.noiseIntensity);
	}
	return density;
}

#endif // COMMON_VOLUMETRIC_FOG_H
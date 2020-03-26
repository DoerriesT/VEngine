#ifndef LIGHTING_H
#define LIGHTING_H

#include "shadingModel.hlsli"

struct LightingParams
{
	float3 albedo;
	float3 N;
	float3 V;
	float3 viewSpacePosition;
	float metalness;
	float roughness;
};

float smoothDistanceAtt(float squaredDistance, float invSqrAttRadius)
{
	float factor = squaredDistance * invSqrAttRadius;
	float smoothFactor = saturate(1.0 - factor * factor);
	return smoothFactor * smoothFactor;
}

float getDistanceAtt(float3 unnormalizedLightVector, float invSqrAttRadius)
{
	float sqrDist = dot(unnormalizedLightVector, unnormalizedLightVector);
	float attenuation = 1.0 / (max(sqrDist, invSqrAttRadius));
	attenuation *= smoothDistanceAtt(sqrDist, invSqrAttRadius);
	
	return attenuation;
}

float getAngleAtt(float3 L, float3 lightDir, float lightAngleScale, float lightAngleOffset)
{
	float cd = dot(lightDir, L);
	float attenuation = saturate(cd * lightAngleScale + lightAngleOffset);
	attenuation *= attenuation;
	
	return attenuation;
}

float3 evaluatePointLight(const LightingParams params, const PointLightData pointLightData)
{
	const float3 unnormalizedLightVector = pointLightData.positionRadius.xyz - params.viewSpacePosition;
	const float3 L = normalize(unnormalizedLightVector);
	const float att = getDistanceAtt(unnormalizedLightVector, pointLightData.colorInvSqrAttRadius.w);
	const float3 radiance = pointLightData.colorInvSqrAttRadius.rgb * att;

	const float3 F0 = lerp(0.04, params.albedo, params.metalness);
	return Default_Lit(params.albedo, F0, radiance, params.N, params.V, L, params.roughness, params.metalness);
}

float3 evaluateSpotLight(const LightingParams params, const SpotLightData spotLightData)
{
	const float3 unnormalizedLightVector = spotLightData.positionAngleScale.xyz - params.viewSpacePosition;
	const float3 L = normalize(unnormalizedLightVector);
	const float att = getDistanceAtt(unnormalizedLightVector, spotLightData.colorInvSqrAttRadius.w)
					* getAngleAtt(L, spotLightData.directionAngleOffset.xyz, spotLightData.positionAngleScale.w, spotLightData.directionAngleOffset.w);
	const float3 radiance = spotLightData.colorInvSqrAttRadius.rgb * att;
	
	const float3 F0 = lerp(0.04, params.albedo, params.metalness);
	return Default_Lit(params.albedo, F0, radiance, params.N, params.V, L, params.roughness, params.metalness);
}

float3 evaluateDirectionalLight(const LightingParams params, const DirectionalLightData directionalLightData)
{
	const float3 F0 = lerp(0.04, params.albedo, params.metalness);
	return Default_Lit(params.albedo, F0, directionalLightData.color.rgb, params.N, params.V, directionalLightData.direction.xyz, params.roughness, params.metalness);
}

uint getTileAddress(uint2 pixelCoord, uint width, uint wordCount)
{
	uint2 tile = pixelCoord / TILE_SIZE;
	uint address = tile.x + tile.y * ((width + TILE_SIZE - 1) / TILE_SIZE);
	return address * wordCount;
}

#endif // LIGHTING_H
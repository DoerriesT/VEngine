#ifndef COMMON_LIGHTING_H
#define COMMON_LIGHTING_H

#include "shadingModel.h"

#ifndef DIFFUSE_ONLY
#define DIFFUSE_ONLY 0
#endif // DIFFUSE_ONLY

struct LightingParams
{
	vec3 albedo;
	vec3 N;
	vec3 V;
	vec3 viewSpacePosition;
	float metalness;
	float roughness;
};

float smoothDistanceAtt(float squaredDistance, float invSqrAttRadius)
{
	float factor = squaredDistance * invSqrAttRadius;
	float smoothFactor = clamp(1.0 - factor * factor, 0.0, 1.0);
	return smoothFactor * smoothFactor;
}

float getDistanceAtt(vec3 unnormalizedLightVector, float invSqrAttRadius)
{
	float sqrDist = dot(unnormalizedLightVector, unnormalizedLightVector);
	float attenuation = 1.0 / (max(sqrDist, invSqrAttRadius));
	attenuation *= smoothDistanceAtt(sqrDist, invSqrAttRadius);
	
	return attenuation;
}

float getAngleAtt(vec3 L, vec3 lightDir, float lightAngleScale, float lightAngleOffset)
{
	float cd = dot(lightDir, L);
	float attenuation = clamp(cd * lightAngleScale + lightAngleOffset, 0.0, 1.0);
	attenuation *= attenuation;
	
	return attenuation;
}	

vec3 evaluatePointLight(const LightingParams params, const PointLightData pointLightData)
{
	const vec3 unnormalizedLightVector = pointLightData.positionRadius.xyz - params.viewSpacePosition;
	const vec3 L = normalize(unnormalizedLightVector);
	const float att = getDistanceAtt(unnormalizedLightVector, pointLightData.colorInvSqrAttRadius.w);
	const vec3 radiance = pointLightData.colorInvSqrAttRadius.rgb * att;

#if DIFFUSE_ONLY
	return Diffuse_Lit(params.albedo, radiance, params.N, L);
#else
	const vec3 F0 = mix(vec3(0.04), params.albedo, params.metalness);
	return Default_Lit(params.albedo, F0, radiance, params.N, params.V, L, params.roughness, params.metalness);
#endif // DIFFUSE_ONLY
}

vec3 evaluateSpotLight(const LightingParams params, const SpotLightData spotLightData)
{
	const vec3 unnormalizedLightVector = spotLightData.positionAngleScale.xyz - params.viewSpacePosition;
	const vec3 L = normalize(unnormalizedLightVector);
	const float att = getDistanceAtt(unnormalizedLightVector, spotLightData.colorInvSqrAttRadius.w)
					* getAngleAtt(L, spotLightData.directionAngleOffset.xyz, spotLightData.positionAngleScale.w, spotLightData.directionAngleOffset.w);
	const vec3 radiance = spotLightData.colorInvSqrAttRadius.rgb * att;
	
#if DIFFUSE_ONLY
	return Diffuse_Lit(params.albedo, radiance, params.N, L);
#else
	const vec3 F0 = mix(vec3(0.04), params.albedo, params.metalness);
	return Default_Lit(params.albedo, F0, radiance, params.N, params.V, L, params.roughness, params.metalness);
#endif // DIFFUSE_ONLY
}

vec3 evaluateDirectionalLight(const LightingParams params, const DirectionalLightData directionalLightData)
{
#if DIFFUSE_ONLY
	return Diffuse_Lit(params.albedo, directionalLightData.color.rgb, params.N, directionalLightData.direction.xyz);
#else
	const vec3 F0 = mix(vec3(0.04), params.albedo, params.metalness);
	return Default_Lit(params.albedo, F0, directionalLightData.color.rgb, params.N, params.V, directionalLightData.direction.xyz, params.roughness, params.metalness);
#endif // DIFFUSE_ONLY
}

uint getTileAddress(uvec2 pixelCoord, uint width, uint wordCount)
{
	uvec2 tile = pixelCoord / TILE_SIZE;
	uint address = tile.x + tile.y * (width / TILE_SIZE + ((width % TILE_SIZE == 0) ? 0 : 1));
	return address * wordCount;
}

#endif // COMMON_LIGHTING_H
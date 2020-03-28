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

float3 evaluatePointLight(const LightingParams params, const PointLight pointLight)
{
	const float3 unnormalizedLightVector = pointLight.position - params.viewSpacePosition;
	const float3 L = normalize(unnormalizedLightVector);
	const float att = getDistanceAtt(unnormalizedLightVector, pointLight.invSqrAttRadius);
	const float3 radiance = pointLight.color * att;

	const float3 F0 = lerp(0.04, params.albedo, params.metalness);
	return Default_Lit(params.albedo, F0, radiance, params.N, params.V, L, params.roughness, params.metalness);
}

float3 evaluateSpotLight(const LightingParams params, const SpotLight spotLight)
{
	const float3 unnormalizedLightVector = spotLight.position - params.viewSpacePosition;
	const float3 L = normalize(unnormalizedLightVector);
	const float att = getDistanceAtt(unnormalizedLightVector, spotLight.invSqrAttRadius)
					* getAngleAtt(L, spotLight.direction, spotLight.angleScale, spotLight.angleOffset);
	const float3 radiance = spotLight.color * att;
	
	const float3 F0 = lerp(0.04, params.albedo, params.metalness);
	return Default_Lit(params.albedo, F0, radiance, params.N, params.V, L, params.roughness, params.metalness);
}

float3 evaluateDirectionalLight(const LightingParams params, const DirectionalLight directionalLight)
{
	const float3 F0 = lerp(0.04, params.albedo, params.metalness);
	return Default_Lit(params.albedo, F0, directionalLight.color, params.N, params.V, directionalLight.direction, params.roughness, params.metalness);
}

uint getTileAddress(uint2 pixelCoord, uint width, uint wordCount)
{
	uint2 tile = pixelCoord / TILE_SIZE;
	uint address = tile.x + tile.y * ((width + TILE_SIZE - 1) / TILE_SIZE);
	return address * wordCount;
}

void getLightingMinMaxIndices(ByteAddressBuffer zBinBuffer, uint itemCount, float linearDepth, out uint minIdx, out uint maxIdx, out uint wordMin, out uint wordMax, out uint wordCount)
{
	wordMin = 0;
	wordCount = (itemCount + 31) / 32;
	wordMax = wordCount - 1;
	
	const uint zBinAddress = min(uint(linearDepth), 8191u);
	const uint zBinData = zBinBuffer.Load(zBinAddress * 4);
	const uint minIndex = zBinData >> 16;
	const uint maxIndex = zBinData & uint(0xFFFF);
	// mergedMin scalar from this point
	const uint mergedMin = WaveReadLaneFirst(WaveActiveMin(minIndex)); 
	// mergedMax scalar from this point
	const uint mergedMax = WaveReadLaneFirst(WaveActiveMax(maxIndex)); 
	wordMin = max(mergedMin / 32, wordMin);
	wordMax = min(mergedMax / 32, wordMax);
	
	minIdx = minIndex;
	maxIdx = maxIndex;
}

uint getLightingBitMask(ByteAddressBuffer bitMaskBuffer, uint tileAddress, uint wordIndex, uint minIndex, uint maxIndex)
{
	uint mask = bitMaskBuffer.Load((tileAddress + wordIndex) * 4);
	
	// mask by zbin mask
	const int localBaseIndex = int(wordIndex * 32);
	const uint localMin = clamp(int(minIndex) - localBaseIndex, 0, 31);
	const uint localMax = clamp(int(maxIndex) - localBaseIndex, 0, 31);
	const uint maskWidth = localMax - localMin + 1;
	const uint zBinMask = (maskWidth == 32) ? uint(0xFFFFFFFF) : (((1 << maskWidth) - 1) << localMin);
	mask &= zBinMask;
	
	// compact word bitmask over all threads in subgroup
	uint mergedMask = WaveReadLaneFirst(WaveActiveBitOr(mask));
	
	return mergedMask;
}

#endif // LIGHTING_H
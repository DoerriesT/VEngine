#include "bindingHelper.hlsli"
#include "packing.hlsli"
#include "common.hlsli"
#include "volumetricFogScatter.hlsli"

#define VOLUME_DEPTH (64)
#define VOLUME_NEAR (0.5)
#define VOLUME_FAR (64.0)

RWTexture3D<float4> g_ResultImage : REGISTER_UAV(RESULT_IMAGE_BINDING, RESULT_IMAGE_SET);
Texture3D<float4> g_PrevResultImage : REGISTER_SRV(PREV_RESULT_IMAGE_BINDING, PREV_RESULT_IMAGE_SET);
Texture3D<float4> g_ScatteringExtinctionImage : REGISTER_SRV(SCATTERING_EXTINCTION_IMAGE_BINDING, SCATTERING_EXTINCTION_IMAGE_SET);
Texture3D<float4> g_EmissivePhaseImage : REGISTER_SRV(EMISSIVE_PHASE_IMAGE_BINDING, EMISSIVE_PHASE_IMAGE_SET);
Texture2DArray<float4> g_ShadowImage : REGISTER_SRV(SHADOW_IMAGE_BINDING, SHADOW_IMAGE_SET);
SamplerState g_LinearSampler : REGISTER_SAMPLER(LINEAR_SAMPLER_BINDING, LINEAR_SAMPLER_SET);
SamplerComparisonState g_ShadowSampler : REGISTER_SAMPLER(SHADOW_SAMPLER_BINDING, SHADOW_SAMPLER_SET);
StructuredBuffer<float4x4> g_ShadowMatrices : REGISTER_SRV(SHADOW_MATRICES_BINDING, SHADOW_MATRICES_SET);
ConstantBuffer<Constants> g_Constants : REGISTER_CBV(CONSTANT_BUFFER_BINDING, CONSTANT_BUFFER_SET);
ByteAddressBuffer g_PointLightZBins : REGISTER_SRV(POINT_LIGHT_Z_BINS_BUFFER_BINDING, POINT_LIGHT_Z_BINS_BUFFER_SET);
ByteAddressBuffer g_PointBitMask : REGISTER_SRV(POINT_LIGHT_BIT_MASK_BUFFER_BINDING, POINT_LIGHT_BIT_MASK_BUFFER_SET);
StructuredBuffer<PointLightData> g_PointLightData : REGISTER_SRV(POINT_LIGHT_DATA_BINDING, POINT_LIGHT_DATA_SET);
ByteAddressBuffer g_SpotLightZBins : REGISTER_SRV(SPOT_LIGHT_Z_BINS_BUFFER_BINDING, SPOT_LIGHT_Z_BINS_BUFFER_SET);
ByteAddressBuffer g_SpotBitMask : REGISTER_SRV(SPOT_LIGHT_BIT_MASK_BUFFER_BINDING, SPOT_LIGHT_BIT_MASK_BUFFER_SET);
StructuredBuffer<SpotLightData> g_SpotLightData : REGISTER_SRV(SPOT_LIGHT_DATA_BINDING, SPOT_LIGHT_DATA_SET);

//PUSH_CONSTS(PushConsts, g_PushConsts);

float getViewSpaceDistance(float texelDepth)
{
	float d = texelDepth * (1.0 / VOLUME_DEPTH);
	return VOLUME_NEAR * exp2(d * (log2(VOLUME_FAR / VOLUME_NEAR)));
}

float3 calcWorldSpacePos(float3 texelCoord)
{
	uint3 imageDims;
	g_ResultImage.GetDimensions(imageDims.x, imageDims.y, imageDims.z);
	float2 uv = texelCoord.xy / float2(imageDims.xy);
	
	float3 pos = lerp(g_Constants.frustumCornerTL, g_Constants.frustumCornerTR, uv.x);
	pos = lerp(pos, lerp(g_Constants.frustumCornerBL, g_Constants.frustumCornerBR, uv.x), uv.y);
	
	pos *= getViewSpaceDistance(texelCoord.z) / VOLUME_FAR;
	
	pos += g_Constants.cameraPos;
	
	return pos;
}

float3 getSunLightingRadiance(float3 worldSpacePos)
{
	float4 tc = -1.0;
	const int cascadeDataOffset = g_Constants.cascadeOffset;
	for (int i = g_Constants.cascadeCount - 1; i >= 0; --i)
	{
		const float4 shadowCoord = float4(mul(g_ShadowMatrices[cascadeDataOffset + i], float4(worldSpacePos, 1.0)).xyz, cascadeDataOffset + i);
		const bool valid = all(abs(shadowCoord.xy) < 0.97);
		tc = valid ? shadowCoord : tc;
	}
	
	tc.xy = tc.xy * 0.5 + 0.5;
	const float shadow = tc.w != -1.0 ? g_ShadowImage.SampleCmpLevelZero(g_ShadowSampler, tc.xyw, tc.z) : 0.0;
	
	return g_Constants.sunLightRadiance * (1.0 - shadow);
}

float henyeyGreenstein(float3 V, float3 L, float g)
{
	float num = -g * g + 1.0;
	float cosTheta = dot(L, V);
	float denom = 4.0 * PI * pow((g * g + 1.0) - 2.0 * g * cosTheta, 1.5);
	return num / denom;
}

uint getTileAddress(uint2 coord, uint width, uint wordCount)
{
	uint2 tile = (coord * 8 + (TILE_SIZE - 1)) / TILE_SIZE;
	uint address = tile.x + tile.y * ((width * 8 + (TILE_SIZE - 1)) / TILE_SIZE);
	return address * wordCount;
}

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

[numthreads(2, 2, 16)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	uint3 froxelID = threadID.xyz;
	// world-space position of volumetric texture texel
	const float3 worldSpacePos = calcWorldSpacePos(froxelID + float3(g_Constants.jitterX, g_Constants.jitterY, g_Constants.jitterZ));
	const float3 viewSpacePos = mul(g_Constants.viewMatrix, float4(worldSpacePos, 1.0)).xyz;
	
	// normalized view dir
	const float3 V = normalize(g_Constants.cameraPos.xyz - worldSpacePos);
	
	const float4 scatteringExtinction = g_ScatteringExtinctionImage.Load(int4(froxelID.xyz, 0));
	const float4 emissivePhase = g_EmissivePhaseImage.Load(int4(froxelID.xyz, 0));
	
	// integrate inscattered lighting
	float3 lighting = emissivePhase.rgb;
	{
		// directional light
		lighting += getSunLightingRadiance(worldSpacePos) * henyeyGreenstein(V, g_Constants.sunDirection.xyz, emissivePhase.w);
		
		// point lights
		if (g_Constants.pointLightCount > 0)
		{
			const float3 viewSpaceV = normalize(-viewSpacePos);
			uint wordMin = 0;
			const uint wordCount = (g_Constants.pointLightCount + 31) / 32;
			uint wordMax = wordCount - 1;
			
			const uint zBinAddress = clamp(uint(floor(-viewSpacePos.z)), 0, 8191);
			const uint zBinData = g_PointLightZBins.Load(zBinAddress * 4);
			const uint minIndex = (zBinData & uint(0xFFFF0000)) >> 16;
			const uint maxIndex = zBinData & uint(0xFFFF);
			// mergedMin scalar from this point
			const uint mergedMin = WaveReadLaneFirst(WaveActiveMin(minIndex)); 
			// mergedMax scalar from this point
			const uint mergedMax = WaveReadLaneFirst(WaveActiveMax(maxIndex)); 
			wordMin = max(mergedMin / 32, wordMin);
			wordMax = min(mergedMax / 32, wordMax);
			uint3 imageDims;
			g_ResultImage.GetDimensions(imageDims.x, imageDims.y, imageDims.z);
			const uint address = getTileAddress(froxelID.xy, imageDims.x, wordCount);
			
			for (uint wordIndex = wordMin; wordIndex <= wordMax; ++wordIndex)
			{
				uint mask = g_PointBitMask.Load((address + wordIndex) * 4);
				
				// mask by zbin mask
				const int localBaseIndex = int(wordIndex * 32);
				const uint localMin = clamp(int(minIndex) - localBaseIndex, 0, 31);
				const uint localMax = clamp(int(maxIndex) - localBaseIndex, 0, 31);
				const uint maskWidth = localMax - localMin + 1;
				const uint zBinMask = (maskWidth == 32) ? uint(0xFFFFFFFF) : (((1 << maskWidth) - 1) << localMin);
				mask &= zBinMask;
				
				// compact word bitmask over all threads in subgroup
				uint mergedMask = WaveReadLaneFirst(WaveActiveBitOr(mask));
				
				while (mergedMask != 0)
				{
					const uint bitIndex = firstbitlow(mergedMask);
					const uint index = 32 * wordIndex + bitIndex;
					mergedMask ^= (1 << bitIndex);
					
					PointLightData pointLightData = g_PointLightData[index];
					const float3 unnormalizedLightVector = pointLightData.positionRadius.xyz - viewSpacePos;
					const float3 L = normalize(unnormalizedLightVector);
					const float att = getDistanceAtt(unnormalizedLightVector, pointLightData.colorInvSqrAttRadius.w);
					const float3 radiance = pointLightData.colorInvSqrAttRadius.rgb * att;
					
					lighting += radiance * henyeyGreenstein(viewSpaceV, L, emissivePhase.w);
				}
			}
		}
		
		// spot lights
		if (g_Constants.spotLightCount > 0)
		{
			const float3 viewSpaceV = normalize(-viewSpacePos);
			uint wordMin = 0;
			const uint wordCount = (g_Constants.spotLightCount + 31) / 32;
			uint wordMax = wordCount - 1;
			
			const uint zBinAddress = clamp(uint(floor(-viewSpacePos.z)), 0, 8191);
			const uint zBinData = g_SpotLightZBins.Load(zBinAddress * 4);
			const uint minIndex = (zBinData & uint(0xFFFF0000)) >> 16;
			const uint maxIndex = zBinData & uint(0xFFFF);
			// mergedMin scalar from this point
			const uint mergedMin = WaveReadLaneFirst(WaveActiveMin(minIndex)); 
			// mergedMax scalar from this point
			const uint mergedMax = WaveReadLaneFirst(WaveActiveMax(maxIndex)); 
			wordMin = max(mergedMin / 32, wordMin);
			wordMax = min(mergedMax / 32, wordMax);
			uint3 imageDims;
			g_ResultImage.GetDimensions(imageDims.x, imageDims.y, imageDims.z);
			const uint address = getTileAddress(froxelID.xy, imageDims.x, wordCount);
			
			for (uint wordIndex = wordMin; wordIndex <= wordMax; ++wordIndex)
			{
				uint mask = g_SpotBitMask.Load((address + wordIndex) * 4);
				
				// mask by zbin mask
				const int localBaseIndex = int(wordIndex * 32);
				const uint localMin = clamp(int(minIndex) - localBaseIndex, 0, 31);
				const uint localMax = clamp(int(maxIndex) - localBaseIndex, 0, 31);
				const uint maskWidth = localMax - localMin + 1;
				const uint zBinMask = (maskWidth == 32) ? uint(0xFFFFFFFF) : (((1 << maskWidth) - 1) << localMin);
				mask &= zBinMask;
				
				// compact word bitmask over all threads in subgroup
				uint mergedMask = WaveReadLaneFirst(WaveActiveBitOr(mask));
				
				while (mergedMask != 0)
				{
					const uint bitIndex = firstbitlow(mergedMask);
					const uint index = 32 * wordIndex + bitIndex;
					mergedMask ^= (1 << bitIndex);
					
					SpotLightData spotLightData = g_SpotLightData[index];
					const float3 unnormalizedLightVector = spotLightData.positionAngleScale.xyz - viewSpacePos;
					const float3 L = normalize(unnormalizedLightVector);
					const float att = getDistanceAtt(unnormalizedLightVector, spotLightData.colorInvSqrAttRadius.w)
									* getAngleAtt(L, spotLightData.directionAngleOffset.xyz, spotLightData.positionAngleScale.w, spotLightData.directionAngleOffset.w);
					const float3 radiance = spotLightData.colorInvSqrAttRadius.rgb * att;
					
					lighting += radiance * henyeyGreenstein(viewSpaceV, L, emissivePhase.w);
				}
			}
		}
	}
	
	const float3 albedo = scatteringExtinction.rgb / max(scatteringExtinction.w, 1e-7);
	lighting *= albedo;
	
	float4 result = float4(lighting, scatteringExtinction.w);
	
	// reproject and combine with previous result from previous frame
	{
		float4 prevViewSpacePos = mul(g_Constants.prevViewMatrix, float4(calcWorldSpacePos(froxelID + 0.5), 1.0));
		
		float z = -prevViewSpacePos.z;
		float d = (log2(max(0, z * (1.0 / VOLUME_NEAR))) * (1.0 / log2(VOLUME_FAR / VOLUME_NEAR)));

		float4 prevClipSpacePos = mul(g_Constants.prevProjMatrix, prevViewSpacePos);
		float3 prevTexCoord = float3((prevClipSpacePos.xy / prevClipSpacePos.w) * 0.5 + 0.5, d);
		prevTexCoord.xy = prevTexCoord.xy * g_Constants.reprojectedTexCoordScaleBias.xy + g_Constants.reprojectedTexCoordScaleBias.zw;
		
		bool validCoord = all(prevTexCoord >= 0.0 && prevTexCoord <= 1.0);
		float4 prevResult = validCoord ? g_PrevResultImage.SampleLevel(g_LinearSampler, prevTexCoord, 0.0) : 0.0;
		
		result = lerp(prevResult, result, validCoord ? 0.015 : 1.0);
	}
	
	g_ResultImage[froxelID] = result;
}
#include "bindingHelper.hlsli"
#include "volumetricFogScatter.hlsli"
#include "packing.hlsli"
#include "common.hlsli"
#include "lighting.hlsli"
#include "commonVolumetricFog.hlsli"
#include "commonEncoding.hlsli"
#include "commonFourierOpacity.hlsli"

#ifndef CHECKER_BOARD
#define CHECKER_BOARD 0
#endif // CHECKER_BOARD

#ifndef VBUFFER_ONLY
#define VBUFFER_ONLY 0
#endif // VBUFFER_ONLY

#ifndef IN_SCATTER_ONLY
#define IN_SCATTER_ONLY 0
#endif // IN_SCATTER_ONLY

RWTexture3D<float4> g_ResultImage : REGISTER_UAV(RESULT_IMAGE_BINDING, 0);
Texture3D<float4> g_HistoryImage : REGISTER_SRV(HISTORY_IMAGE_BINDING, 0);
ConstantBuffer<Constants> g_Constants : REGISTER_CBV(CONSTANT_BUFFER_BINDING, 0);
Texture2DArray<float4> g_ShadowImage : REGISTER_SRV(SHADOW_IMAGE_BINDING, 0);
Texture2D<float> g_ShadowAtlasImage : REGISTER_SRV(SHADOW_ATLAS_IMAGE_BINDING, 0);
StructuredBuffer<float4x4> g_ShadowMatrices : REGISTER_SRV(SHADOW_MATRICES_BINDING, 0);
ByteAddressBuffer g_ExposureData : REGISTER_SRV(EXPOSURE_DATA_BUFFER_BINDING, 0);
Texture2DArray<float4> g_FomImage : REGISTER_SRV(FOM_IMAGE_BINDING, 0);
Texture2DArray<float4> g_FomDirectionalImage : REGISTER_SRV(FOM_DIRECTIONAL_IMAGE_BINDING, 0);
Texture2DArray<float> g_FomDirectionalDepthRangeImage : REGISTER_SRV(FOM_DIRECTIONAL_DEPTH_RANGE_IMAGE_BINDING, 0);

StructuredBuffer<GlobalParticipatingMedium> g_GlobalMedia : REGISTER_SRV(GLOBAL_MEDIA_BINDING, 0);

// local media
StructuredBuffer<LocalParticipatingMedium> g_LocalMedia : REGISTER_SRV(LOCAL_MEDIA_BINDING, 0);
Texture2DArray<uint> g_LocalMediaBitMaskImage : REGISTER_SRV(LOCAL_MEDIA_BIT_MASK_BINDING, 0);
ByteAddressBuffer g_LocalMediaDepthBins : REGISTER_SRV(LOCAL_MEDIA_Z_BINS_BINDING, 0);

// directional lights
StructuredBuffer<DirectionalLight> g_DirectionalLights : REGISTER_SRV(DIRECTIONAL_LIGHTS_BINDING, 0);
StructuredBuffer<DirectionalLight> g_DirectionalLightsShadowed : REGISTER_SRV(DIRECTIONAL_LIGHTS_SHADOWED_BINDING, 0);

// punctual lights
StructuredBuffer<PunctualLight> g_PunctualLights : REGISTER_SRV(PUNCTUAL_LIGHTS_BINDING, 0);
Texture2DArray<uint> g_PunctualLightsBitMaskImage : REGISTER_SRV(PUNCTUAL_LIGHTS_BIT_MASK_BINDING, 0);
ByteAddressBuffer g_PunctualLightsDepthBins : REGISTER_SRV(PUNCTUAL_LIGHTS_Z_BINS_BINDING, 0);

// punctual lights shadowed
StructuredBuffer<PunctualLightShadowed> g_PunctualLightsShadowed : REGISTER_SRV(PUNCTUAL_LIGHTS_SHADOWED_BINDING, 0);
Texture2DArray<uint> g_PunctualLightsShadowedBitMaskImage : REGISTER_SRV(PUNCTUAL_LIGHTS_SHADOWED_BIT_MASK_BINDING, 0);
ByteAddressBuffer g_PunctualLightsShadowedDepthBins : REGISTER_SRV(PUNCTUAL_LIGHTS_SHADOWED_Z_BINS_BINDING, 0);


Texture3D<float4> g_Textures3D[TEXTURE_ARRAY_SIZE] : REGISTER_SRV(0, 1);

SamplerState g_Samplers[SAMPLER_COUNT] : REGISTER_SAMPLER(0, 2);
SamplerComparisonState g_ShadowSampler : REGISTER_SAMPLER(0, 3);

//PUSH_CONSTS(PushConsts, g_PushConsts);

float3 calcWorldSpacePos(float3 texelCoord)
{
	uint3 imageDims;
	g_ResultImage.GetDimensions(imageDims.x, imageDims.y, imageDims.z);
	float2 uv = texelCoord.xy / float2(imageDims.xy);
	
	float3 pos = lerp(g_Constants.frustumCornerTL, g_Constants.frustumCornerTR, uv.x);
	pos = lerp(pos, lerp(g_Constants.frustumCornerBL, g_Constants.frustumCornerBR, uv.x), uv.y);
	
	float d = texelCoord.z * rcp(g_Constants.volumeDepth);
	float z = g_Constants.volumeNear * exp2(d * (log2(g_Constants.volumeFar / g_Constants.volumeNear)));
	pos *= z / g_Constants.volumeFar;
	
	pos += g_Constants.cameraPos;
	
	return pos;
}

float getDirectionalLightShadow(const DirectionalLight directionalLight, float3 worldSpacePos)
{
	float4 tc = -1.0;
	const int cascadeDataOffset = directionalLight.shadowOffset;
	int cascadeCount = directionalLight.shadowCount;
	for (int i = cascadeCount - 1; i >= 0; --i)
	{
		const float4 shadowCoord = float4(mul(g_ShadowMatrices[cascadeDataOffset + i], float4(worldSpacePos, 1.0)).xyz, cascadeDataOffset + i);
		const bool valid = all(abs(shadowCoord.xy) < 0.97);
		tc = valid ? shadowCoord : tc;
	}
	
	tc.xy = tc.xy * float2(0.5, -0.5) + 0.5;
	float shadow = tc.w != -1.0 ? g_ShadowImage.SampleCmpLevelZero(g_ShadowSampler, tc.xyw, tc.z) : 0.0;
	
	if (shadow > 0.0 && g_Constants.volumetricShadow )
	{
		float4 fom0 = g_FomDirectionalImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], float3(tc.xy, tc.w * 2.0 + 0.0), 0.0);
		float4 fom1 = g_FomDirectionalImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], float3(tc.xy, tc.w * 2.0 + 1.0), 0.0);
		
		float rangeBegin = g_FomDirectionalDepthRangeImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], float3(tc.xy, tc.w * 2), 0.0).x;
		float rangeEnd = g_FomDirectionalDepthRangeImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], float3(tc.xy, tc.w * 2 + 1), 0.0).x;
		float depth = clamp(tc.z, rangeBegin, rangeEnd);
		depth = (depth - rangeBegin) / (rangeEnd - rangeBegin);
		
		shadow = min(shadow, fourierOpacityGetTransmittance(depth, fom0, fom1));
	}
	
	return shadow;
}

float henyeyGreenstein(float3 V, float3 L, float g)
{
	float num = -g * g + 1.0;
	float cosTheta = dot(-L, V);
	float denom = 4.0 * PI * pow((g * g + 1.0) - 2.0 * g * cosTheta, 1.5);
	return num / denom;
}

void vbuffer(uint2 coord, float3 worldSpacePos, float linearDepth, out float3 scattering, out float extinction, out float3 emissive, out float phase)
{
	scattering = 0.0;
	extinction = 0.0;
	emissive = 0.0;
	phase = 0.0;
	uint accumulatedMediaCount = 0;
	
	// iterate over all global participating media
	{
		for (int i = 0; i < g_Constants.globalMediaCount; ++i)
		{
			GlobalParticipatingMedium medium = g_GlobalMedia[i];
			const float density = volumetricFogGetDensity(medium, worldSpacePos, g_Textures3D, g_Samplers[SAMPLER_LINEAR_CLAMP]);
			scattering += medium.scattering * density;
			extinction += medium.extinction * density;
			emissive += medium.emissive * density;
			phase += medium.phase;
			++accumulatedMediaCount;
		}
	}

	// iterate over all local participating media
	uint localMediaCount = g_Constants.localMediaCount;
	if (localMediaCount > 0)
	{
		uint wordMin, wordMax, minIndex, maxIndex, wordCount;
		getLightingMinMaxIndices(g_LocalMediaDepthBins, localMediaCount, linearDepth, minIndex, maxIndex, wordMin, wordMax, wordCount);
		const uint2 tile = getTile(coord / g_Constants.volumeResResultRes.xy * g_Constants.volumeResResultRes.zw);
	
		for (uint wordIndex = wordMin; wordIndex <= wordMax; ++wordIndex)
		{
			uint mask = getLightingBitMask(g_LocalMediaBitMaskImage, tile, wordIndex, minIndex, maxIndex);
			
			while (mask != 0)
			{
				const uint bitIndex = firstbitlow(mask);
				const uint index = 32 * wordIndex + bitIndex;
				mask ^= (1 << bitIndex);
				
				LocalParticipatingMedium medium = g_LocalMedia[index];
				
				const float3 localPos = float3(dot(medium.worldToLocal0, float4(worldSpacePos, 1.0)), 
								dot(medium.worldToLocal1, float4(worldSpacePos, 1.0)), 
								dot(medium.worldToLocal2, float4(worldSpacePos, 1.0)));
								
				if (all(abs(localPos) <= 1.0) && (medium.spherical == 0 || dot(localPos, localPos) <= 1.0))
				{
					float density = volumetricFogGetDensity(medium, localPos, g_Textures3D, g_Samplers[SAMPLER_LINEAR_CLAMP]);
					scattering += medium.scattering * density;
					extinction += medium.extinction * density;
					emissive += medium.emissive * density;
					phase += medium.phase;
					++accumulatedMediaCount;
				}
			}
		}
	}
	
	phase = accumulatedMediaCount > 0 ? phase * rcp((float)accumulatedMediaCount) : 0.0;
}

float4 inscattering(uint2 coord, float3 V, float3 worldSpacePos, float linearDepth, float3 scattering, float extinction, float3 emissive, float phase)
{
	// integrate inscattered lighting
	float3 lighting = emissive;
	{
		// ambient
		{
			float3 ambientLight = (1.0 / PI);
			lighting += ambientLight / (4.0 * PI);
		}
		
		// directional lights
		{
			for (uint i = 0; i < g_Constants.directionalLightCount; ++i)
			{
				DirectionalLight directionalLight = g_DirectionalLights[i];
				lighting += directionalLight.color * henyeyGreenstein(V, directionalLight.direction, phase);
			}
		}
		
		// shadowed directional lights
		{
			for (uint i = 0; i < g_Constants.directionalLightShadowedCount; ++i)
			{
				DirectionalLight directionalLight = g_DirectionalLightsShadowed[i];
				float shadow = getDirectionalLightShadow(directionalLight, worldSpacePos);
				lighting += directionalLight.color * henyeyGreenstein(V, directionalLight.direction, phase) * shadow;
			}
		}
		
		// punctual lights
		uint punctualLightCount = g_Constants.punctualLightCount;
		if (punctualLightCount > 0)
		{
			uint wordMin, wordMax, minIndex, maxIndex, wordCount;
			getLightingMinMaxIndices(g_PunctualLightsDepthBins, punctualLightCount, linearDepth, minIndex, maxIndex, wordMin, wordMax, wordCount);
			const uint2 tile = getTile(coord / g_Constants.volumeResResultRes.xy * g_Constants.volumeResResultRes.zw);
	
			for (uint wordIndex = wordMin; wordIndex <= wordMax; ++wordIndex)
			{
				uint mask = getLightingBitMask(g_PunctualLightsBitMaskImage, tile, wordIndex, minIndex, maxIndex);
				
				while (mask != 0)
				{
					const uint bitIndex = firstbitlow(mask);
					const uint index = 32 * wordIndex + bitIndex;
					mask ^= (1 << bitIndex);
					
					PunctualLight light = g_PunctualLights[index];
					
					const float3 unnormalizedLightVector = light.position - worldSpacePos;
					const float3 L = normalize(unnormalizedLightVector);
					float att = getDistanceAtt(unnormalizedLightVector, light.invSqrAttRadius);
					
					if (light.angleScale != -1.0) // -1.0 is a special value that marks this light as a point light
					{
						att *= getAngleAtt(L, light.direction, light.angleScale, light.angleOffset);
					}
					
					const float3 radiance = light.color * att;
					
					lighting += radiance * henyeyGreenstein(V, L, phase);
				}
			}
		}
		
		// punctual lights shadowed
		uint punctualLightShadowedCount = g_Constants.punctualLightShadowedCount;
		if (punctualLightShadowedCount > 0)
		{
			uint wordMin, wordMax, minIndex, maxIndex, wordCount;
			getLightingMinMaxIndices(g_PunctualLightsShadowedDepthBins, punctualLightShadowedCount, linearDepth, minIndex, maxIndex, wordMin, wordMax, wordCount);
			const uint2 tile = getTile(coord / g_Constants.volumeResResultRes.xy * g_Constants.volumeResResultRes.zw);
	
			for (uint wordIndex = wordMin; wordIndex <= wordMax; ++wordIndex)
			{
				uint mask = getLightingBitMask(g_PunctualLightsShadowedBitMaskImage, tile, wordIndex, minIndex, maxIndex);
				
				while (mask != 0)
				{
					const uint bitIndex = firstbitlow(mask);
					const uint index = 32 * wordIndex + bitIndex;
					mask ^= (1 << bitIndex);
					
					PunctualLightShadowed lightShadowed = g_PunctualLightsShadowed[index];
					
					// evaluate shadow
					float4 shadowPos;
					
					// spot light
					if (lightShadowed.light.angleScale != -1.0)
					{
						shadowPos.x = dot(lightShadowed.shadowMatrix0, float4(worldSpacePos, 1.0));
						shadowPos.y = dot(lightShadowed.shadowMatrix1, float4(worldSpacePos, 1.0));
						shadowPos.z = dot(lightShadowed.shadowMatrix2, float4(worldSpacePos, 1.0));
						shadowPos.w = dot(lightShadowed.shadowMatrix3, float4(worldSpacePos, 1.0));
						shadowPos.xyz /= shadowPos.w;
						shadowPos.xy = shadowPos.xy * float2(0.5, -0.5) + 0.5;
						shadowPos.xy = shadowPos.xy * lightShadowed.shadowAtlasParams[0].x + lightShadowed.shadowAtlasParams[0].yz;
					}
					// point light
					else
					{
						float3 lightToPoint = worldSpacePos - lightShadowed.light.position;
						int faceIdx = 0;
						shadowPos.xy = sampleCube(lightToPoint, faceIdx);
						// scale down the coord to account for the border area required for filtering
						shadowPos.xy = ((shadowPos.xy * 2.0 - 1.0) * lightShadowed.shadowAtlasParams[faceIdx].w) * 0.5 + 0.5;
						shadowPos.x = 1.0 - shadowPos.x; // correct for handedness (cubemap coordinate system is left-handed, our world space is right-handed)
						shadowPos.xy = shadowPos.xy * lightShadowed.shadowAtlasParams[faceIdx].x + lightShadowed.shadowAtlasParams[faceIdx].yz;
						
						float dist = faceIdx < 2 ? abs(lightToPoint.x) : faceIdx < 4 ? abs(lightToPoint.y) : abs(lightToPoint.z);
						const float nearPlane = 0.1f;
						float param0 = -lightShadowed.radius / (lightShadowed.radius - nearPlane);
						float param1 = param0 * nearPlane;
						shadowPos.z = -param0 + param1 / dist;
					}
					
					float shadow = g_ShadowAtlasImage.SampleCmpLevelZero(g_ShadowSampler, shadowPos.xy, shadowPos.z).x;
					
					const float3 unnormalizedLightVector = lightShadowed.light.position - worldSpacePos;
					const float3 L = normalize(unnormalizedLightVector);
					float att = getDistanceAtt(unnormalizedLightVector, lightShadowed.light.invSqrAttRadius);
					
					if (lightShadowed.light.angleScale != -1.0) // -1.0 is a special value that marks this light as a point light
					{
						att *= getAngleAtt(L, lightShadowed.light.direction, lightShadowed.light.angleScale, lightShadowed.light.angleOffset);
					}
					
					// get volumetric shadow
					if (shadow > 0.0 && att > 0.0 && g_Constants.volumetricShadow && lightShadowed.fomShadowAtlasParams.w != 0.0)
					{
						float2 uv;
						// spot light
						if (lightShadowed.light.angleScale != -1.0)
						{
							uv.x = dot(lightShadowed.shadowMatrix0, float4(worldSpacePos, 1.0));
							uv.y = dot(lightShadowed.shadowMatrix1, float4(worldSpacePos, 1.0));
							uv /= dot(lightShadowed.shadowMatrix3, float4(worldSpacePos, 1.0));
						}
						// point light
						else
						{
							uv = encodeOctahedron(normalize(lightShadowed.light.position - worldSpacePos));
						}
						
						uv = uv * 0.5 + 0.5;
						uv = uv * lightShadowed.fomShadowAtlasParams.x + lightShadowed.fomShadowAtlasParams.yz;
						
						float4 fom0 = g_FomImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], float3(uv, 0.0), 0.0);
						float4 fom1 = g_FomImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], float3(uv, 1.0), 0.0);
						
						float depth = distance(worldSpacePos, lightShadowed.light.position) * rcp(lightShadowed.radius);
						
						shadow *= fourierOpacityGetTransmittance(depth, fom0, fom1);
					}
					
					const float3 radiance = lightShadowed.light.color * att;
					
					lighting += shadow * radiance * henyeyGreenstein(V, L, phase);
				}
			}
		}
	}
	
	float4 result = float4(lighting * scattering, extinction);
	
	// apply pre-exposure
	result.rgb *= asfloat(g_ExposureData.Load(0));
	
	return result;
}

float4 temporalFilter(uint3 threadID, float4 result)
{
	float4 prevViewSpacePos = mul(g_Constants.prevViewMatrix, float4(calcWorldSpacePos(threadID + 0.5), 1.0));
	
	float z = -prevViewSpacePos.z;
	float d = log2(max(0, z * rcp(g_Constants.volumeNear))) * rcp(log2(g_Constants.volumeFar / g_Constants.volumeNear));

	float4 prevClipSpacePos = mul(g_Constants.prevProjMatrix, prevViewSpacePos);
	float3 prevTexCoord = float3((prevClipSpacePos.xy / prevClipSpacePos.w) * float2(0.5, -0.5) + 0.5, d);
	
	bool validCoord = all(prevTexCoord >= 0.0 && prevTexCoord <= 1.0);
	float4 prevResult = 0.0;
	if (validCoord)
	{
		prevResult = g_HistoryImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], prevTexCoord, 0.0);
		
		// prevResult.rgb is pre-exposed -> convert from previous frame exposure to current frame exposure
		prevResult.rgb *= asfloat(g_ExposureData.Load(1 << 2)); // 0 = current frame exposure | 1 = previous frame to current frame exposure
		
		result = lerp(prevResult, result, g_Constants.alpha);
	}
	
	return result;
}

[numthreads(4, 4, 4)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	float3 texelCoord = threadID;
#if CHECKER_BOARD
	texelCoord.z *= 2.0;
	texelCoord.z += (((threadID.x + threadID.y) & 1) == g_Constants.checkerBoardCondition) ? 1.0 : 0.0;
#endif // CHECKER_BOARD
	texelCoord += float3(g_Constants.jitterX, g_Constants.jitterY, frac(g_Constants.jitterZ));
	
	const float3 worldSpacePos = calcWorldSpacePos(texelCoord);
	const float linearDepth = -dot(g_Constants.viewMatrixDepthRow, float4(worldSpacePos, 1.0));

	float3 scattering = 0.0;
	float extinction = 0.0;
	float3 emissive = 0.0;
	float phase = 0.0;
	
	vbuffer(threadID.xy, worldSpacePos, linearDepth, scattering, extinction, emissive, phase);
	
	const float3 V = normalize(g_Constants.cameraPos - worldSpacePos);
	float4 result = inscattering(threadID.xy, V, worldSpacePos, linearDepth, scattering, extinction, emissive, phase);
	
#if !CHECKER_BOARD
	// reproject and combine with previous result from previous frame
	if (g_Constants.ignoreHistory == 0)
	{
		result = temporalFilter(threadID, result);
	}
#endif // !CHECKER_BOARD
	
	g_ResultImage[threadID] = result;
}
#include "bindingHelper.hlsli"
#include "volumetricFogMerged.hlsli"
#include "packing.hlsli"
#include "common.hlsli"
#include "lighting.hlsli"
#include "commonVolumetricFog.hlsli"
#include "commonEncoding.hlsli"
#include "commonFourierOpacity.hlsli"

#define VOLUME_DEPTH (64)
#define VOLUME_NEAR (0.5)
#define VOLUME_FAR (64.0)

RWTexture3D<float4> g_ResultImage : REGISTER_UAV(RESULT_IMAGE_BINDING, 0);
ConstantBuffer<Constants> g_Constants : REGISTER_CBV(CONSTANT_BUFFER_BINDING, 0);
SamplerState g_LinearSampler : REGISTER_SAMPLER(LINEAR_SAMPLER_BINDING, 0);
Texture2DArray<float4> g_ShadowImage : REGISTER_SRV(SHADOW_IMAGE_BINDING, 0);
Texture2D<float> g_ShadowAtlasImage : REGISTER_SRV(SHADOW_ATLAS_IMAGE_BINDING, 0);
SamplerComparisonState g_ShadowSampler : REGISTER_SAMPLER(SHADOW_SAMPLER_BINDING, 0);
StructuredBuffer<float4x4> g_ShadowMatrices : REGISTER_SRV(SHADOW_MATRICES_BINDING, 0);
ByteAddressBuffer g_ExposureData : REGISTER_SRV(EXPOSURE_DATA_BUFFER_BINDING, 0);
Texture2DArray<float4> g_fomImage : REGISTER_SRV(FOM_IMAGE_BINDING, 0);

StructuredBuffer<GlobalParticipatingMedium> g_GlobalMedia : REGISTER_SRV(GLOBAL_MEDIA_BINDING, 0);

// local media
StructuredBuffer<LocalParticipatingMedium> g_LocalMedia : REGISTER_SRV(LOCAL_MEDIA_BINDING, 0);
ByteAddressBuffer g_LocalMediaBitMask : REGISTER_SRV(LOCAL_MEDIA_BIT_MASK_BINDING, 0);
ByteAddressBuffer g_LocalMediaDepthBins : REGISTER_SRV(LOCAL_MEDIA_Z_BINS_BINDING, 0);

// directional lights
StructuredBuffer<DirectionalLight> g_DirectionalLights : REGISTER_SRV(DIRECTIONAL_LIGHTS_BINDING, 0);
StructuredBuffer<DirectionalLight> g_DirectionalLightsShadowed : REGISTER_SRV(DIRECTIONAL_LIGHTS_SHADOWED_BINDING, 0);

// punctual lights
StructuredBuffer<PunctualLight> g_PunctualLights : REGISTER_SRV(PUNCTUAL_LIGHTS_BINDING, 0);
ByteAddressBuffer g_PunctualLightsBitMask : REGISTER_SRV(PUNCTUAL_LIGHTS_BIT_MASK_BINDING, 0);
ByteAddressBuffer g_PunctualLightsDepthBins : REGISTER_SRV(PUNCTUAL_LIGHTS_Z_BINS_BINDING, 0);

// punctual lights shadowed
StructuredBuffer<PunctualLightShadowed> g_PunctualLightsShadowed : REGISTER_SRV(PUNCTUAL_LIGHTS_SHADOWED_BINDING, 0);
ByteAddressBuffer g_PunctualLightsShadowedBitMask : REGISTER_SRV(PUNCTUAL_LIGHTS_SHADOWED_BIT_MASK_BINDING, 0);
ByteAddressBuffer g_PunctualLightsShadowedDepthBins : REGISTER_SRV(PUNCTUAL_LIGHTS_SHADOWED_Z_BINS_BINDING, 0);


Texture3D<float4> g_Textures3D[TEXTURE_ARRAY_SIZE] : REGISTER_SRV(0, 1);

//PUSH_CONSTS(PushConsts, g_PushConsts);

float3 calcWorldSpacePos(float3 texelCoord)
{
	uint3 imageDims;
	g_ResultImage.GetDimensions(imageDims.x, imageDims.y, imageDims.z);
	float2 uv = texelCoord.xy / float2(imageDims.xy);
	
	float3 pos = lerp(g_Constants.frustumCornerTL, g_Constants.frustumCornerTR, uv.x);
	pos = lerp(pos, lerp(g_Constants.frustumCornerBL, g_Constants.frustumCornerBR, uv.x), uv.y);
	
	float d = texelCoord.z * (1.0 / VOLUME_DEPTH);
	float z = VOLUME_NEAR * exp2(d * (log2(VOLUME_FAR / VOLUME_NEAR)));
	pos *= z / VOLUME_FAR;
	
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
	const float shadow = tc.w != -1.0 ? g_ShadowImage.SampleCmpLevelZero(g_ShadowSampler, tc.xyw, tc.z) : 0.0;
	
	return shadow;
}

float henyeyGreenstein(float3 V, float3 L, float g)
{
	float num = -g * g + 1.0;
	float cosTheta = dot(-L, V);
	float denom = 4.0 * PI * pow((g * g + 1.0) - 2.0 * g * cosTheta, 1.5);
	return num / denom;
}

[numthreads(4, 4, 4)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	//float4x4 ditherMatrix = ((float4x4(0, 8, 2, 10,
	//								12, 4, 14, 6,
	//								3, 11, 1, 9,
	//								15, 7, 13, 5) + 1.0) * (1.0 / 16.0));
	//float dither = ditherMatrix[threadID.y % 4][threadID.x % 4];
	
	float2x2 ditherMatrix = float2x2(0.25, 0.75, 1.0, 0.5);
	float dither = ditherMatrix[threadID.y % 2][threadID.x % 2];
	
	dither = g_Constants.useDithering != 0 ? dither : 0.0;
	
	const float3 worldSpacePos0 = calcWorldSpacePos(threadID + float3(g_Constants.jitterX, g_Constants.jitterY, frac(g_Constants.jitterZ + dither)));
	const float3 viewSpacePos0 = mul(g_Constants.viewMatrix, float4(worldSpacePos0, 1.0)).xyz;
	
	const float3 worldSpacePos1 = calcWorldSpacePos(threadID + float3(g_Constants.jitter1.x, g_Constants.jitter1.y, frac(g_Constants.jitter1.z + dither)));
	const float3 viewSpacePos1 = mul(g_Constants.viewMatrix, float4(worldSpacePos1, 1.0)).xyz;

	const float3 worldSpacePos[] = { worldSpacePos0, worldSpacePos1 };
	const float3 viewSpacePos[] = { viewSpacePos0, viewSpacePos1 };

	uint3 imageDims;
	g_ResultImage.GetDimensions(imageDims.x, imageDims.y, imageDims.z);
	uint targetImageWidth = imageDims.x * 8;
	
	float3 scattering = 0.0;
	float extinction = 0.0;
	float3 emissive = 0.0;
	float phase = 0.0;
	uint accumulatedMediaCount = 0;
	
	// iterate over all global participating media
	{
		for (int i = 0; i < g_Constants.globalMediaCount; ++i)
		{
			GlobalParticipatingMedium medium = g_GlobalMedia[i];
			const float density = volumetricFogGetDensity(medium, worldSpacePos[0], g_Textures3D, g_LinearSampler);
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
		getLightingMinMaxIndicesRange(g_LocalMediaDepthBins, localMediaCount, -viewSpacePos[0].z, -viewSpacePos[1].z, minIndex, maxIndex, wordMin, wordMax, wordCount);
		const uint address = getTileAddress(threadID.xy * 8, targetImageWidth, wordCount);
	
		for (uint wordIndex = wordMin; wordIndex <= wordMax; ++wordIndex)
		{
			uint mask = getLightingBitMask(g_LocalMediaBitMask, address, wordIndex, minIndex, maxIndex);
			
			while (mask != 0)
			{
				const uint bitIndex = firstbitlow(mask);
				const uint index = 32 * wordIndex + bitIndex;
				mask ^= (1 << bitIndex);
				
				LocalParticipatingMedium medium = g_LocalMedia[index];
				
				int count = g_Constants.sampleCount;
				for (int i = 0; i < count; ++i)
				{
					const float3 localPos = float3(dot(medium.worldToLocal0, float4(worldSpacePos[i], 1.0)), 
									dot(medium.worldToLocal1, float4(worldSpacePos[i], 1.0)), 
									dot(medium.worldToLocal2, float4(worldSpacePos[i], 1.0)));
									
					if (all(abs(localPos) <= 1.0))
					{
						float multiplier = (count == 2) ? 0.5 : 1.0;
						float density = volumetricFogGetDensity(medium, localPos, g_Textures3D, g_LinearSampler);
						scattering += medium.scattering * multiplier * density;
						extinction += medium.extinction * multiplier * density;
						emissive += medium.emissive * multiplier * density;
						phase += medium.phase;
						++accumulatedMediaCount;
					}
				}
			}
		}
	}
	
	phase = accumulatedMediaCount > 0 ? phase * rcp((float)accumulatedMediaCount) : 0.0;
	
	const float4 scatteringExtinction = float4(scattering, extinction);
	const float4 emissivePhase = float4(emissive, phase);
	
	const float3 viewSpaceV[] = { normalize(-viewSpacePos[0]), normalize(-viewSpacePos[1]) };
	
	// integrate inscattered lighting
	float3 lighting = emissivePhase.rgb;
	{
		// ambient
		{
			float3 ambientLight = (1.0 / PI);
			lighting += ambientLight * (1.0 / (4.0 * PI));
		}
		
		// directional lights
		{
			for (uint i = 0; i < g_Constants.directionalLightCount; ++i)
			{
				DirectionalLight directionalLight = g_DirectionalLights[i];
				int count = g_Constants.sampleCount;
				for (int j = 0; j < count; ++j)
				{
					float multiplier = (count == 2) ? 0.5 : 1.0;
					lighting += directionalLight.color * henyeyGreenstein(viewSpaceV[j], directionalLight.direction, emissivePhase.w) * multiplier;
				}
			}
		}
		
		// shadowed directional lights
		{
			for (uint i = 0; i < g_Constants.directionalLightShadowedCount; ++i)
			{
				DirectionalLight directionalLight = g_DirectionalLightsShadowed[i];
				int count = g_Constants.sampleCount;
				for (int j = 0; j < count; ++j)
				{
					float multiplier = (count == 2) ? 0.5 : 1.0;
					float shadow = getDirectionalLightShadow(directionalLight, worldSpacePos[j]);
					lighting += directionalLight.color * henyeyGreenstein(viewSpaceV[j], directionalLight.direction, emissivePhase.w) * shadow * multiplier;
				}
			}
		}
		
		// punctual lights
		uint punctualLightCount = g_Constants.punctualLightCount;
		if (punctualLightCount > 0)
		{
			uint wordMin, wordMax, minIndex, maxIndex, wordCount;
			getLightingMinMaxIndicesRange(g_PunctualLightsDepthBins, punctualLightCount, -viewSpacePos[0].z, -viewSpacePos[1].z, minIndex, maxIndex, wordMin, wordMax, wordCount);
			const uint address = getTileAddress(threadID.xy * 8, targetImageWidth, wordCount);
	
			for (uint wordIndex = wordMin; wordIndex <= wordMax; ++wordIndex)
			{
				uint mask = getLightingBitMask(g_PunctualLightsBitMask, address, wordIndex, minIndex, maxIndex);
				
				while (mask != 0)
				{
					const uint bitIndex = firstbitlow(mask);
					const uint index = 32 * wordIndex + bitIndex;
					mask ^= (1 << bitIndex);
					
					PunctualLight light = g_PunctualLights[index];
					
					int count = g_Constants.sampleCount;
					for (int i = 0; i < count; ++i)
					{
						const float3 unnormalizedLightVector = light.position - viewSpacePos[i];
						const float3 L = normalize(unnormalizedLightVector);
						float att = getDistanceAtt(unnormalizedLightVector, light.invSqrAttRadius);
						
						if (light.angleScale != -1.0) // -1.0 is a special value that marks this light as a point light
						{
							att *= getAngleAtt(L, light.direction, light.angleScale, light.angleOffset);
						}
						
						float multiplier = (count == 2) ? 0.5 : 1.0;
						const float3 radiance = light.color * att * multiplier;
						
						lighting += radiance * henyeyGreenstein(viewSpaceV[i], L, emissivePhase.w);
					}
				}
			}
		}
		
		// punctual lights shadowed
		uint punctualLightShadowedCount = g_Constants.punctualLightShadowedCount;
		if (punctualLightShadowedCount > 0)
		{
			uint wordMin, wordMax, minIndex, maxIndex, wordCount;
			getLightingMinMaxIndicesRange(g_PunctualLightsShadowedDepthBins, punctualLightShadowedCount, -viewSpacePos[0].z, -viewSpacePos[1].z, minIndex, maxIndex, wordMin, wordMax, wordCount);
			const uint address = getTileAddress(threadID.xy * 8, targetImageWidth, wordCount);
	
			for (uint wordIndex = wordMin; wordIndex <= wordMax; ++wordIndex)
			{
				uint mask = getLightingBitMask(g_PunctualLightsShadowedBitMask, address, wordIndex, minIndex, maxIndex);
				
				while (mask != 0)
				{
					const uint bitIndex = firstbitlow(mask);
					const uint index = 32 * wordIndex + bitIndex;
					mask ^= (1 << bitIndex);
					
					PunctualLightShadowed lightShadowed = g_PunctualLightsShadowed[index];
					
					int count = g_Constants.sampleCount;
					for (int i = 0; i < count; ++i)
					{
						// evaluate shadow
						float4 shadowPos;
						
						// spot light
						if (lightShadowed.light.angleScale != -1.0)
						{
							shadowPos.x = dot(lightShadowed.shadowMatrix0, float4(worldSpacePos[i], 1.0));
							shadowPos.y = dot(lightShadowed.shadowMatrix1, float4(worldSpacePos[i], 1.0));
							shadowPos.z = dot(lightShadowed.shadowMatrix2, float4(worldSpacePos[i], 1.0));
							shadowPos.w = dot(lightShadowed.shadowMatrix3, float4(worldSpacePos[i], 1.0));
							shadowPos.xyz /= shadowPos.w;
							shadowPos.xy = shadowPos.xy * float2(0.5, -0.5) + 0.5;
							shadowPos.xy = shadowPos.xy * lightShadowed.shadowAtlasParams[0].x + lightShadowed.shadowAtlasParams[0].yz;
						}
						// point light
						else
						{
							float3 lightToPoint = worldSpacePos[i] - lightShadowed.positionWS;
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
						
						const float3 unnormalizedLightVector = lightShadowed.light.position - viewSpacePos[i];
						const float3 L = normalize(unnormalizedLightVector);
						float att = getDistanceAtt(unnormalizedLightVector, lightShadowed.light.invSqrAttRadius);
						
						if (lightShadowed.light.angleScale != -1.0) // -1.0 is a special value that marks this light as a point light
						{
							att *= getAngleAtt(L, lightShadowed.light.direction, lightShadowed.light.angleScale, lightShadowed.light.angleOffset);
						}
						
						// trace extinction volume
						if (shadow > 0.0 && att > 0.0 && g_Constants.volumetricShadow && lightShadowed.fomShadowAtlasParams.w != 0.0)
						{
							//if (g_Constants.volumetricShadow == 2)
							//{
							//	shadow *= raymarch(worldSpacePos[i], lightShadowed.positionWS);
							//}
							//else
							{
								float2 uv;
								// spot light
								if (lightShadowed.light.angleScale != -1.0)
								{
									uv.x = dot(lightShadowed.shadowMatrix0, float4(worldSpacePos[i], 1.0));
									uv.y = dot(lightShadowed.shadowMatrix1, float4(worldSpacePos[i], 1.0));
									uv /= dot(lightShadowed.shadowMatrix3, float4(worldSpacePos[i], 1.0));
								}
								// point light
								else
								{
									uv = encodeOctahedron(normalize(lightShadowed.positionWS - worldSpacePos[i]));
								}
								
								uv = uv * 0.5 + 0.5;
								uv = uv * lightShadowed.fomShadowAtlasParams.x + lightShadowed.fomShadowAtlasParams.yz;
								
								float4 fom0 = g_fomImage.SampleLevel(g_LinearSampler, float3(uv, 0.0), 0.0);
								float4 fom1 = g_fomImage.SampleLevel(g_LinearSampler, float3(uv, 1.0), 0.0);
								
								float depth = distance(worldSpacePos[i], lightShadowed.positionWS) * rcp(lightShadowed.radius);
								//depth = saturate(depth);
							
								shadow *= fourierOpacityGetTransmittance(depth, fom0, fom1);
							}
						}
						
						float multiplier = (count == 2) ? 0.5 : 1.0;
						const float3 radiance = lightShadowed.light.color * att * multiplier;
						
						lighting += shadow * radiance * henyeyGreenstein(viewSpaceV[i], L, emissivePhase.w);
					}
				}
			}
		}
	}
	
	lighting *= scatteringExtinction.rgb;
	
	// apply pre-exposure
	lighting *= asfloat(g_ExposureData.Load(0));
	
	g_ResultImage[threadID] = float4(lighting, scatteringExtinction.w);
}
#include "bindingHelper.hlsli"
#include "volumetricFogScatter.hlsli"
#include "packing.hlsli"
#include "common.hlsli"
#include "lighting.hlsli"


#define VOLUME_DEPTH (64)
#define VOLUME_NEAR (0.5)
#define VOLUME_FAR (64.0)

RWTexture3D<float4> g_ResultImage : REGISTER_UAV(RESULT_IMAGE_BINDING, RESULT_IMAGE_SET);
Texture3D<float4> g_ScatteringExtinctionImage : REGISTER_SRV(SCATTERING_EXTINCTION_IMAGE_BINDING, SCATTERING_EXTINCTION_IMAGE_SET);
Texture3D<float4> g_EmissivePhaseImage : REGISTER_SRV(EMISSIVE_PHASE_IMAGE_BINDING, EMISSIVE_PHASE_IMAGE_SET);
Texture2DArray<float4> g_ShadowImage : REGISTER_SRV(SHADOW_IMAGE_BINDING, SHADOW_IMAGE_SET);
Texture2D<float> g_ShadowAtlasImage : REGISTER_SRV(SHADOW_ATLAS_IMAGE_BINDING, SHADOW_ATLAS_IMAGE_SET);
SamplerComparisonState g_ShadowSampler : REGISTER_SAMPLER(SHADOW_SAMPLER_BINDING, SHADOW_SAMPLER_SET);
StructuredBuffer<float4x4> g_ShadowMatrices : REGISTER_SRV(SHADOW_MATRICES_BINDING, SHADOW_MATRICES_SET);
ConstantBuffer<Constants> g_Constants : REGISTER_CBV(CONSTANT_BUFFER_BINDING, CONSTANT_BUFFER_SET);
ByteAddressBuffer g_ExposureData : REGISTER_SRV(EXPOSURE_DATA_BUFFER_BINDING, EXPOSURE_DATA_BUFFER_SET);

// directional lights
StructuredBuffer<DirectionalLight> g_DirectionalLights : REGISTER_SRV(DIRECTIONAL_LIGHTS_BINDING, DIRECTIONAL_LIGHTS_SET);
StructuredBuffer<DirectionalLight> g_DirectionalLightsShadowed : REGISTER_SRV(DIRECTIONAL_LIGHTS_SHADOWED_BINDING, DIRECTIONAL_LIGHTS_SHADOWED_SET);

// punctual lights
StructuredBuffer<PunctualLight> g_PunctualLights : REGISTER_SRV(PUNCTUAL_LIGHTS_BINDING, PUNCTUAL_LIGHTS_SET);
ByteAddressBuffer g_PunctualLightsBitMask : REGISTER_SRV(PUNCTUAL_LIGHTS_BIT_MASK_BINDING, PUNCTUAL_LIGHTS_BIT_MASK_SET);
ByteAddressBuffer g_PunctualLightsDepthBins : REGISTER_SRV(PUNCTUAL_LIGHTS_Z_BINS_BINDING, PUNCTUAL_LIGHTS_Z_BINS_SET);

// punctual lights shadowed
StructuredBuffer<PunctualLightShadowed> g_PunctualLightsShadowed : REGISTER_SRV(PUNCTUAL_LIGHTS_SHADOWED_BINDING, PUNCTUAL_LIGHTS_SHADOWED_SET);
ByteAddressBuffer g_PunctualLightsShadowedBitMask : REGISTER_SRV(PUNCTUAL_LIGHTS_SHADOWED_BIT_MASK_BINDING, PUNCTUAL_LIGHTS_SHADOWED_BIT_MASK_SET);
ByteAddressBuffer g_PunctualLightsShadowedDepthBins : REGISTER_SRV(PUNCTUAL_LIGHTS_SHADOWED_Z_BINS_BINDING, PUNCTUAL_LIGHTS_SHADOWED_Z_BINS_SET);

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
	
	tc.xy = tc.xy * 0.5 + 0.5;
	const float shadow = tc.w != -1.0 ? g_ShadowImage.SampleCmpLevelZero(g_ShadowSampler, tc.xyw, tc.z) : 0.0;
	
	return shadow;
}

float henyeyGreenstein(float3 V, float3 L, float g)
{
	float num = -g * g + 1.0;
	float cosTheta = dot(L, V);
	float denom = 4.0 * PI * pow((g * g + 1.0) - 2.0 * g * cosTheta, 1.5);
	return num / denom;
}

[numthreads(2, 2, 16)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	uint3 froxelID = threadID.xyz;
	
	//float4x4 ditherMatrix = ((float4x4(0, 8, 2, 10,
	//								12, 4, 14, 6,
	//								3, 11, 1, 9,
	//								15, 7, 13, 5) + 1.0) * (1.0 / 16.0));
	//float dither = ditherMatrix[threadID.y % 4][threadID.x % 4];
	
	float2x2 ditherMatrix = float2x2(0.25, 0.75, 1.0, 0.5);
	float dither = ditherMatrix[threadID.y % 2][threadID.x % 2];
	
	// world-space position of volumetric texture texel
	const float3 worldSpacePos = calcWorldSpacePos(froxelID + float3(g_Constants.jitterX, g_Constants.jitterY, frac(g_Constants.jitterZ + dither)));
	const float3 viewSpacePos = mul(g_Constants.viewMatrix, float4(worldSpacePos, 1.0)).xyz;
	
	const float4 scatteringExtinction = g_ScatteringExtinctionImage.Load(int4(froxelID.xyz, 0));
	const float4 emissivePhase = g_EmissivePhaseImage.Load(int4(froxelID.xyz, 0));
	
	const float3 viewSpaceV = normalize(-viewSpacePos);
	uint3 imageDims;
	g_ResultImage.GetDimensions(imageDims.x, imageDims.y, imageDims.z);
	uint targetImageWidth = imageDims.x * 8;
	
	// integrate inscattered lighting
	float3 lighting = emissivePhase.rgb;
	{
		// directional lights
		{
			for (uint i = 0; i < g_Constants.directionalLightCount; ++i)
			{
				DirectionalLight directionalLight = g_DirectionalLights[i];
				lighting += directionalLight.color * henyeyGreenstein(viewSpaceV, directionalLight.direction, emissivePhase.w);
			}
		}
		
		// shadowed directional lights
		{
			for (uint i = 0; i < g_Constants.directionalLightShadowedCount; ++i)
			{
				DirectionalLight directionalLight = g_DirectionalLightsShadowed[i];
				lighting += directionalLight.color
							* henyeyGreenstein(viewSpaceV, directionalLight.direction, emissivePhase.w)
							* getDirectionalLightShadow(directionalLight, worldSpacePos);
			}
		}
		
		// punctual lights
		uint punctualLightCount = g_Constants.punctualLightCount;
		if (punctualLightCount > 0)
		{
			uint wordMin, wordMax, minIndex, maxIndex, wordCount;
			getLightingMinMaxIndices(g_PunctualLightsDepthBins, punctualLightCount, -viewSpacePos.z, minIndex, maxIndex, wordMin, wordMax, wordCount);
			const uint address = getTileAddress(froxelID.xy * 8, targetImageWidth, wordCount);
	
			for (uint wordIndex = wordMin; wordIndex <= wordMax; ++wordIndex)
			{
				uint mask = getLightingBitMask(g_PunctualLightsBitMask, address, wordIndex, minIndex, maxIndex);
				
				while (mask != 0)
				{
					const uint bitIndex = firstbitlow(mask);
					const uint index = 32 * wordIndex + bitIndex;
					mask ^= (1 << bitIndex);
					
					PunctualLight light = g_PunctualLights[index];
					
					const float3 unnormalizedLightVector = light.position - viewSpacePos;
					const float3 L = normalize(unnormalizedLightVector);
					float att = getDistanceAtt(unnormalizedLightVector, light.invSqrAttRadius);
					
					if (light.angleScale != -1.0) // -1.0 is a special value that marks this light as a point light
					{
						att *= getAngleAtt(L, light.direction, light.angleScale, light.angleOffset);
					}
					
					const float3 radiance = light.color * att;
					
					lighting += radiance * henyeyGreenstein(viewSpaceV, L, emissivePhase.w);
				}
			}
		}
		
		// punctual lights shadowed
		uint punctualLightShadowedCount = g_Constants.punctualLightShadowedCount;
		if (punctualLightShadowedCount > 0)
		{
			uint wordMin, wordMax, minIndex, maxIndex, wordCount;
			getLightingMinMaxIndices(g_PunctualLightsShadowedDepthBins, punctualLightShadowedCount, -viewSpacePos.z, minIndex, maxIndex, wordMin, wordMax, wordCount);
			const uint address = getTileAddress(froxelID.xy * 8, targetImageWidth, wordCount);
	
			for (uint wordIndex = wordMin; wordIndex <= wordMax; ++wordIndex)
			{
				uint mask = getLightingBitMask(g_PunctualLightsShadowedBitMask, address, wordIndex, minIndex, maxIndex);
				
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
						shadowPos.xy = shadowPos.xy * 0.5 + 0.5;
						shadowPos.xy = shadowPos.xy * lightShadowed.shadowAtlasParams[0].x + lightShadowed.shadowAtlasParams[0].yz;
					}
					// point light
					else
					{
						float3 lightToPoint = worldSpacePos - lightShadowed.positionWS;
						int faceIdx = 0;
						shadowPos.xy = sampleCube(lightToPoint, faceIdx);
						shadowPos.x = 1.0 - shadowPos.x; // correct for handedness (cubemap coordinate system is left-handed, our world space is right-handed)
						shadowPos.xy = shadowPos.xy * lightShadowed.shadowAtlasParams[faceIdx].x + lightShadowed.shadowAtlasParams[faceIdx].yz;
						
						float dist = faceIdx < 2 ? abs(lightToPoint.x) : faceIdx < 4 ? abs(lightToPoint.y) : abs(lightToPoint.z);
						const float nearPlane = 0.1f;
						float param0 = -lightShadowed.radius / (lightShadowed.radius - nearPlane);
						float param1 = param0 * nearPlane;
						shadowPos.z = -param0 + param1 / dist;
					}
					
					float shadow = g_ShadowAtlasImage.SampleCmpLevelZero(g_ShadowSampler, shadowPos.xy, shadowPos.z).x;
					
					const float3 unnormalizedLightVector = lightShadowed.light.position - viewSpacePos;
					const float3 L = normalize(unnormalizedLightVector);
					float att = getDistanceAtt(unnormalizedLightVector, lightShadowed.light.invSqrAttRadius);
					
					if (lightShadowed.light.angleScale != -1.0) // -1.0 is a special value that marks this light as a point light
					{
						att *= getAngleAtt(L, lightShadowed.light.direction, lightShadowed.light.angleScale, lightShadowed.light.angleOffset);
					}
					
					const float3 radiance = lightShadowed.light.color * att;
					
					lighting += shadow * radiance * henyeyGreenstein(viewSpaceV, L, emissivePhase.w);
				}
			}
		}
	}
	
	const float3 albedo = scatteringExtinction.rgb / max(scatteringExtinction.w, 1e-7);
	lighting *= albedo;
	
	// apply pre-exposure
	lighting *= asfloat(g_ExposureData.Load(0));
	
	g_ResultImage[froxelID] = float4(lighting, scatteringExtinction.w);
}
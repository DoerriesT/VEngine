#include "bindingHelper.hlsli"
#include "volumetricFogApply.hlsli"
#include "commonFilter.hlsli"
#include "srgb.hlsli"
#include "common.hlsli"
#include "commonEncoding.hlsli"
#include "lighting.hlsli"

#define VOLUME_DEPTH (64)
#define VOLUME_NEAR (0.5)
#define VOLUME_FAR (64.0)

RWTexture2D<float4> g_ResultImage : REGISTER_UAV(RESULT_IMAGE_BINDING, RESULT_IMAGE_SET);
Texture2D<float4> g_DepthImage : REGISTER_SRV(DEPTH_IMAGE_BINDING, DEPTH_IMAGE_SET);
Texture2D<float4> g_SSAOImage : REGISTER_SRV(SSAO_IMAGE_BINDING, SSAO_IMAGE_SET);
Texture3D<float4> g_VolumetricFogImage : REGISTER_SRV(VOLUMETRIC_FOG_IMAGE_BINDING, VOLUMETRIC_FOG_IMAGE_SET);
Texture2D<float4> g_IndirectSpecularLightImage : REGISTER_SRV(INDIRECT_SPECULAR_LIGHT_IMAGE_BINDING, INDIRECT_SPECULAR_LIGHT_IMAGE_SET);
Texture2D<float2> g_BrdfLutImage : REGISTER_SRV(BRDF_LUT_IMAGE_BINDING, BRDF_LUT_IMAGE_SET);
Texture2D<float4> g_AlbedoMetalnessImage : REGISTER_SRV(ALBEDO_METALNESS_IMAGE_BINDING, ALBEDO_METALNESS_IMAGE_SET);
Texture2D<float4> g_NormalRoughnessImage : REGISTER_SRV(NORMAL_ROUGHNESS_IMAGE_BINDING, NORMAL_ROUGHNESS_IMAGE_SET);
TextureCubeArray<float4> g_ReflectionProbeImage : REGISTER_SRV(REFLECTION_PROBE_IMAGE_BINDING, REFLECTION_PROBE_IMAGE_SET);

StructuredBuffer<LocalReflectionProbe> g_ReflectionProbeData : REGISTER_SRV(REFLECTION_PROBE_DATA_BINDING, REFLECTION_PROBE_DATA_SET);
ByteAddressBuffer g_ReflectionProbeBitMask : REGISTER_SRV(REFLECTION_PROBE_BIT_MASK_BINDING, REFLECTION_PROBE_BIT_MASK_SET);
ByteAddressBuffer g_ReflectionProbeDepthBins : REGISTER_SRV(REFLECTION_PROBE_Z_BINS_BINDING, REFLECTION_PROBE_Z_BINS_SET);

ByteAddressBuffer g_ExposureData : REGISTER_SRV(EXPOSURE_DATA_BUFFER_BINDING, EXPOSURE_DATA_BUFFER_SET);
SamplerState g_LinearSampler : REGISTER_SAMPLER(LINEAR_SAMPLER_BINDING, LINEAR_SAMPLER_SET);

Texture2D<float4> g_Textures[TEXTURE_ARRAY_SIZE] : REGISTER_SRV(TEXTURES_BINDING, TEXTURES_SET);
SamplerState g_Samplers[SAMPLER_COUNT] : REGISTER_SAMPLER(SAMPLERS_BINDING, SAMPLERS_SET);

PUSH_CONSTS(PushConsts, g_PushConsts);

float3 parallaxCorrectReflectionDir(const LocalReflectionProbe probeData, float3 position, float3 reflectedDir)
{
	// Intersection with OBB convert to unit box space
	// Transform in local unit parallax cube space (scaled and rotated)
	const float3 rayLS = float3(dot(probeData.worldToLocal0.xyz, reflectedDir), 
								dot(probeData.worldToLocal1.xyz, reflectedDir), 
								dot(probeData.worldToLocal2.xyz, reflectedDir));
	const float3 invRayLS = 1.0 / rayLS;
	const float3 positionLS = float3(dot(probeData.worldToLocal0, float4(position, 1.0)), 
									dot(probeData.worldToLocal1, float4(position, 1.0)), 
									dot(probeData.worldToLocal2, float4(position, 1.0)));
	
	float3 firstPlaneIntersect  = (1.0 - positionLS) * invRayLS;
	float3 secondPlaneIntersect = (-1.0 - positionLS) * invRayLS;
	float3 furthestPlane = max(firstPlaneIntersect, secondPlaneIntersect);
	float distance = min(furthestPlane.x, min(furthestPlane.y, furthestPlane.z));
	
	// Use Distance in WS directly to recover intersection
	float3 intersectPositionWS = position + reflectedDir * distance;
	
	return intersectPositionWS - probeData.capturePosition;
}

float3 getSpecularDominantDir(float3 N, float3 R, float roughness)
{
	float smoothness = saturate(1.0 - roughness);
	float lerpFactor = smoothness * (sqrt(smoothness) + roughness);
	return lerp(N, R, lerpFactor);
}

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	if (threadID.x >= g_PushConsts.width || threadID.y >= g_PushConsts.height)
	{
		return;
	}

	const float depth = g_DepthImage.Load(int3(threadID.xy, 0)).x;
	const float2 clipSpacePosition = (threadID.xy + 0.5) * float2(g_PushConsts.texelWidth, g_PushConsts.texelHeight) * float2(2.0, -2.0) - float2(1.0, -1.0);
	float4 viewSpacePosition4 = float4(g_PushConsts.unprojectParams.xy * clipSpacePosition, -1.0, g_PushConsts.unprojectParams.z * depth + g_PushConsts.unprojectParams.w);
	float3 viewSpacePosition = viewSpacePosition4.xyz / viewSpacePosition4.w;
	
	float3 result = g_ResultImage[threadID.xy].rgb;
	
	// skip sky pixels
	if (depth != 0.0)
	{
		float4 normalRoughness = g_NormalRoughnessImage.Load(int3(threadID.xy, 0));
		float4 albedoMetalness = approximateSRGBToLinear(g_AlbedoMetalnessImage.Load(int3(threadID.xy, 0)));
		const float3 F0 = lerp(0.04, albedoMetalness.rgb, albedoMetalness.w);
		const float roughness = max(normalRoughness.w, 0.04); // avoid precision problems
		float metalness = albedoMetalness.w;
		float3 albedo = albedoMetalness.rgb;
		const float3 V = -normalize(viewSpacePosition.xyz);
		
		const float3 N = decodeOctahedron24(normalRoughness.xyz);
		
		float4 indirectSpecular = depth == 50.0 ? g_IndirectSpecularLightImage.Load(int3(threadID.xy, 0)) : 0.0;
		
		float3 worldSpacePos = mul(g_PushConsts.invViewMatrix, float4(viewSpacePosition, 1.0)).xyz;
		float3 worldSpaceNormal = mul(g_PushConsts.invViewMatrix, float4(N, 0.0)).xyz;
		float3 worldSpaceViewDir = mul(g_PushConsts.invViewMatrix, float4(V, 0.0)).xyz;
		float3 R = reflect(-worldSpaceViewDir, worldSpaceNormal);
		float3 dominantR = getSpecularDominantDir(worldSpaceNormal, R, roughness);
		
		float weightSum = 0.0;
		float3 reflectionProbeSpecular = 0.0;
		const float preExposureFactor = asfloat(g_ExposureData.Load(0));
		
		// iterate over all reflection probes
		uint probeCount = g_PushConsts.probeCount;
		if (probeCount > 0)
		{
			uint wordMin, wordMax, minIndex, maxIndex, wordCount;
			getLightingMinMaxIndices(g_ReflectionProbeDepthBins, probeCount, -viewSpacePosition.z, minIndex, maxIndex, wordMin, wordMax, wordCount);
			const uint address = getTileAddress(threadID.xy, g_PushConsts.width, wordCount);
		
			const float mipCount = 7.0;
			float mipLevel = roughness * mipCount;
		
			for (uint wordIndex = wordMin; wordIndex <= wordMax; ++wordIndex)
			{
				uint mask = getLightingBitMask(g_ReflectionProbeBitMask, address, wordIndex, minIndex, maxIndex);
				
				while (mask != 0)
				{
					const uint bitIndex = firstbitlow(mask);
					const uint index = 32 * wordIndex + bitIndex;
					mask ^= (1 << bitIndex);
					
					LocalReflectionProbe probeData = g_ReflectionProbeData[index];
					
					const float3 localPos = float3(dot(probeData.worldToLocal0, float4(worldSpacePos, 1.0)), 
									dot(probeData.worldToLocal1, float4(worldSpacePos, 1.0)), 
									dot(probeData.worldToLocal2, float4(worldSpacePos, 1.0)));
									
					if (all(abs(localPos) <= 1.0))
					{
						float3 lookupDir = parallaxCorrectReflectionDir(probeData, worldSpacePos, dominantR);
						lookupDir = lerp(lookupDir, dominantR, roughness);
		
						reflectionProbeSpecular += g_ReflectionProbeImage.SampleLevel(g_LinearSampler, float4(lookupDir, probeData.arraySlot), mipLevel).rgb * preExposureFactor;
						weightSum += 1.0;
					}
				}
			}
		}
		
		reflectionProbeSpecular = weightSum > 0.0 ? reflectionProbeSpecular * (1.0 / weightSum) : reflectionProbeSpecular;

		indirectSpecular.rgb = lerp(reflectionProbeSpecular, indirectSpecular.rgb, indirectSpecular.a);
		
		float2 brdfLut = g_BrdfLutImage.SampleLevel(g_LinearSampler, float2(saturate(dot(N, V)), roughness), 0.0).xy;
		indirectSpecular.rgb *= F0 * brdfLut.x + brdfLut.y;
		
		result += indirectSpecular.rgb;
		
		float ao = 1.0;
		if (g_PushConsts.ssao != 0)
		{
			ao = g_SSAOImage.Load(int3(threadID.xy, 0)).x;
		}
		result += (1.0 / PI) * (1.0 - metalness) * albedo * ao * preExposureFactor;
	}
	
	// volumetric fog
	{
		float z = -viewSpacePosition.z;
		float d = (log2(max(0, z * (1.0 / VOLUME_NEAR))) * (1.0 / log2(VOLUME_FAR / VOLUME_NEAR)));
		
		// the fog image can extend further to the right/downwards than the lighting image, so we cant just use the uv
		// of the current texel but instead need to scale the uv with respect to the fog image resolution
		float3 imageDims;
		g_VolumetricFogImage.GetDimensions(imageDims.x, imageDims.y, imageDims.z);
		float2 scaledFogImageTexelSize = 1.0 / (imageDims.xy * 8.0);
		
		float3 volumetricFogTexCoord = float3((threadID.xy + 0.5) * scaledFogImageTexelSize, d);
		
		float2 noiseTexCoord = float2(threadID.xy + 0.5) * g_PushConsts.noiseScale + (g_PushConsts.noiseJitter);
		float3 noise = g_Textures[g_PushConsts.noiseTexId].SampleLevel(g_Samplers[SAMPLER_POINT_REPEAT], noiseTexCoord, 0.0).xyz;
		
		if (g_PushConsts.useNoise != 0)
		{
			volumetricFogTexCoord += (noise * 2.0 - 1.0) * 1.5 / imageDims;
		}
		
		float4 fog = sampleBicubic(g_VolumetricFogImage, g_LinearSampler, volumetricFogTexCoord.xy, imageDims.xy, 1.0 / imageDims.xy, d);
		//float4 fog = g_VolumetricFogImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], volumetricFogTexCoord, 0.0);
		
		fog.rgb = inverseSimpleTonemap(fog.rgb);
		result = result * fog.aaa + fog.rgb;
	}
	
	g_ResultImage[threadID.xy] = float4(result, 1.0);
}
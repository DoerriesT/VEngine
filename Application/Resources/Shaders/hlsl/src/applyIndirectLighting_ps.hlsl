#include "bindingHelper.hlsli"
#include "applyIndirectLighting.hlsli"
#include "srgb.hlsli"
#include "common.hlsli"
#include "commonEncoding.hlsli"
#include "lighting.hlsli"

struct PSInput
{
	float4 position : SV_Position;
	float2 texCoord : TEXCOORD;
};

Texture2D<float4> g_DepthImage : REGISTER_SRV(DEPTH_IMAGE_BINDING, 0);
Texture2D<float4> g_SSAOImage : REGISTER_SRV(SSAO_IMAGE_BINDING, 0);
Texture2D<float4> g_IndirectSpecularLightImage : REGISTER_SRV(INDIRECT_SPECULAR_LIGHT_IMAGE_BINDING, 0);
Texture2D<float2> g_BrdfLutImage : REGISTER_SRV(BRDF_LUT_IMAGE_BINDING, 0);
Texture2D<float4> g_AlbedoMetalnessImage : REGISTER_SRV(ALBEDO_METALNESS_IMAGE_BINDING, 0);
Texture2D<float4> g_NormalRoughnessImage : REGISTER_SRV(NORMAL_ROUGHNESS_IMAGE_BINDING, 0);
TextureCubeArray<float4> g_ReflectionProbeImage : REGISTER_SRV(REFLECTION_PROBE_IMAGE_BINDING, 0);

StructuredBuffer<LocalReflectionProbe> g_ReflectionProbeData : REGISTER_SRV(REFLECTION_PROBE_DATA_BINDING, 0);
ByteAddressBuffer g_ReflectionProbeBitMask : REGISTER_SRV(REFLECTION_PROBE_BIT_MASK_BINDING, 0);
ByteAddressBuffer g_ReflectionProbeDepthBins : REGISTER_SRV(REFLECTION_PROBE_Z_BINS_BINDING, 0);

ByteAddressBuffer g_ExposureData : REGISTER_SRV(EXPOSURE_DATA_BUFFER_BINDING, 0);
SamplerState g_LinearSampler : REGISTER_SAMPLER(LINEAR_SAMPLER_BINDING, 0);

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

[earlydepthstencil]
float4 main(PSInput input) : SV_Target0
{
	const float depth = g_DepthImage.Load(int3(input.position.xy, 0)).x;
	const float2 clipSpacePosition = input.position.xy * float2(g_PushConsts.texelWidth, g_PushConsts.texelHeight) * float2(2.0, -2.0) - float2(1.0, -1.0);
	float3 viewSpacePosition = float3(g_PushConsts.unprojectParams.xy * clipSpacePosition, -1.0);
	viewSpacePosition /= g_PushConsts.unprojectParams.z * depth + g_PushConsts.unprojectParams.w;
	
	float4 normalRoughness = g_NormalRoughnessImage.Load(int3(input.position.xy, 0));
	float4 albedoMetalness = approximateSRGBToLinear(g_AlbedoMetalnessImage.Load(int3(input.position.xy, 0)));
	const float3 F0 = lerp(0.04, albedoMetalness.rgb, albedoMetalness.w);
	const float roughness = max(normalRoughness.w, 0.04); // avoid precision problems
	float metalness = albedoMetalness.w;
	float3 albedo = albedoMetalness.rgb;
	const float3 V = -normalize(viewSpacePosition.xyz);
	
	const float3 N = decodeOctahedron24(normalRoughness.xyz);
	
	
	// apply indirect specular
	float4 indirectSpecular = 0.0;//depth == 50.0 ? g_IndirectSpecularLightImage.Load(int3(input.position.xy, 0)) : 0.0;
	
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
		const uint address = getTileAddress(int2(input.position.xy), g_PushConsts.width, wordCount);
	
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
	
	float3 result = indirectSpecular.rgb;
	
	// apply indirect diffuse
	float ao = 1.0;
	if (g_PushConsts.ssao != 0)
	{
		ao = g_SSAOImage.Load(int3(input.position.xy, 0)).x;
	}
	result += (1.0 / PI) * (1.0 - metalness) * albedo * ao * preExposureFactor;
	
	return float4(result, 0.0);
}
#include "bindingHelper.hlsli"
#include "packing.hlsli"
#include "common.hlsli"
#include "volumetricFogScatter.hlsli"

#define VOLUME_DEPTH (64)

RWTexture3D<float4> g_ResultImage : REGISTER_UAV(RESULT_IMAGE_BINDING, RESULT_IMAGE_SET);
Texture3D<float4> g_ScatteringExtinctionImage : REGISTER_SRV(SCATTERING_EXTINCTION_IMAGE_BINDING, SCATTERING_EXTINCTION_IMAGE_SET);
Texture3D<float4> g_EmissivePhaseImage : REGISTER_SRV(EMISSIVE_PHASE_IMAGE_BINDING, EMISSIVE_PHASE_IMAGE_SET);
Texture2DArray<float4> g_ShadowImage : REGISTER_SRV(SHADOW_IMAGE_BINDING, SHADOW_IMAGE_SET);
SamplerComparisonState g_ShadowSampler : REGISTER_SAMPLER(SHADOW_SAMPLER_BINDING, SHADOW_SAMPLER_SET);
StructuredBuffer<float4x4> g_ShadowMatrices : REGISTER_SRV(SHADOW_MATRICES_BINDING, SHADOW_MATRICES_SET);

PUSH_CONSTS(PushConsts, g_PushConsts);


float3 calcWorldSpacePos(int3 coord)
{
	float3 texelCoord = coord + 0.5;
	uint3 imageDims;
	g_ResultImage.GetDimensions(imageDims.x, imageDims.y, imageDims.z);
	float2 uv = texelCoord.xy / float2(imageDims.xy);
	
	float3 pos = lerp(g_PushConsts.frustumCornerTL, g_PushConsts.frustumCornerTR, uv.x);
	pos = lerp(pos, lerp(g_PushConsts.frustumCornerBL, g_PushConsts.frustumCornerBR, uv.x), uv.y);
	
	float d = texelCoord.z * (1.0 / VOLUME_DEPTH);
	float n = 0.1;
	float f = 64.0;
	float z = n * exp2(d * (log2(f / n)));
	pos *= z / f;
	
	pos += g_PushConsts.cameraPos;
	
	return pos;
}

float3 getSunLightingRadiance(float3 worldSpacePos)
{
	float4 tc = -1.0;
	const int cascadeDataOffset = g_PushConsts.cascadeOffset;
	for (int i = g_PushConsts.cascadeCount - 1; i >= 0; --i)
	{
		const float4 shadowCoord = float4(mul(g_ShadowMatrices[cascadeDataOffset + i], float4(worldSpacePos, 1.0)).xyz, cascadeDataOffset + i);
		const bool valid = all(abs(shadowCoord.xy) < 0.97);
		tc = valid ? shadowCoord : tc;
	}
	
	tc.xy = tc.xy * 0.5 + 0.5;
	const float shadow = tc.w != -1.0 ? g_ShadowImage.SampleCmpLevelZero(g_ShadowSampler, tc.xyw, tc.z) : 0.0;
	
	return g_PushConsts.sunLightRadiance * (1.0 - shadow);
}

float henyeyGreenstein(float3 V, float3 L, float g)
{
	float num = -g * g + 1.0;
	float cosTheta = dot(L, V);
	float denom = 4.0 * PI * pow((g * g + 1.0) - 2.0 * g * cosTheta, 1.5);
	return num / denom;
}

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	// world-space position of volumetric texture texel
	const float3 worldSpacePos = calcWorldSpacePos(threadID);
	
	// normalized view dir
	const float3 V = normalize(g_PushConsts.cameraPos.xyz - worldSpacePos);
	
	const float4 scatteringExtinction = g_ScatteringExtinctionImage.Load(int4(threadID.xyz, 0));
	const float4 emissivePhase = g_EmissivePhaseImage.Load(int4(threadID.xyz, 0));
	
	// integrate inscattered lighting
	float3 lighting = emissivePhase.rgb;
	{
		// directional light
		lighting += getSunLightingRadiance(worldSpacePos) * henyeyGreenstein(V, g_PushConsts.sunDirection.xyz, emissivePhase.w);
	}
	
	const float3 albedo = scatteringExtinction.rgb / max(scatteringExtinction.w, 1e-7);
	lighting *= albedo;
	
	g_ResultImage[threadID] = float4(lighting, scatteringExtinction.w);
}
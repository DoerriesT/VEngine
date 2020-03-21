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

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	// world-space position of volumetric texture texel
	const float3 worldSpacePos = calcWorldSpacePos(threadID + float3(g_Constants.jitterX, g_Constants.jitterY, g_Constants.jitterZ));
	
	// normalized view dir
	const float3 V = normalize(g_Constants.cameraPos.xyz - worldSpacePos);
	
	const float4 scatteringExtinction = g_ScatteringExtinctionImage.Load(int4(threadID.xyz, 0));
	const float4 emissivePhase = g_EmissivePhaseImage.Load(int4(threadID.xyz, 0));
	
	// integrate inscattered lighting
	float3 lighting = emissivePhase.rgb;
	{
		// directional light
		lighting += getSunLightingRadiance(worldSpacePos) * henyeyGreenstein(V, g_Constants.sunDirection.xyz, emissivePhase.w);
	}
	
	const float3 albedo = scatteringExtinction.rgb / max(scatteringExtinction.w, 1e-7);
	lighting *= albedo;
	
	float4 result = float4(lighting, scatteringExtinction.w);
	
	// reproject and combine with previous result from previous frame
	{
		float4 prevViewSpacePos = mul(g_Constants.prevViewMatrix, float4(calcWorldSpacePos(threadID + 0.5), 1.0));
		
		float z = -prevViewSpacePos.z;
		float d = (log2(max(0, z * (1.0 / VOLUME_NEAR))) * (1.0 / log2(VOLUME_FAR / VOLUME_NEAR)));

		float4 prevClipSpacePos = mul(g_Constants.prevProjMatrix, prevViewSpacePos);
		float3 prevTexCoord = float3((prevClipSpacePos.xy / prevClipSpacePos.w) * 0.5 + 0.5, d);
		prevTexCoord.xy = prevTexCoord.xy * g_Constants.reprojectedTexCoordScaleBias.xy + g_Constants.reprojectedTexCoordScaleBias.zw;
		
		bool validCoord = all(prevTexCoord >= 0.0 && prevTexCoord <= 1.0);
		float4 prevResult = validCoord ? g_PrevResultImage.SampleLevel(g_LinearSampler, prevTexCoord, 0.0) : 0.0;
		
		result = lerp(prevResult, result, validCoord ? 0.015 : 1.0);
	}
	
	g_ResultImage[threadID] = result;
}
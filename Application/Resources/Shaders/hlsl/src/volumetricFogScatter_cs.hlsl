#include "bindingHelper.hlsli"
#include "packing.hlsli"
#include "common.hlsli"
#include "volumetricFogScatter.hlsli"

RWTexture3D<float4> g_ResultImage : REGISTER_UAV(RESULT_IMAGE_BINDING, RESULT_IMAGE_SET);
Texture2DArray<float4> g_ShadowImage : REGISTER_SRV(SHADOW_IMAGE_BINDING, SHADOW_IMAGE_SET);
SamplerComparisonState g_ShadowSampler : REGISTER_SAMPLER(SHADOW_SAMPLER_BINDING, SHADOW_SAMPLER_SET);
StructuredBuffer<float4x4> g_ShadowMatrices : REGISTER_SRV(SHADOW_MATRICES_BINDING, SHADOW_MATRICES_SET);

PUSH_CONSTS(PushConsts, g_PushConsts);

float computeLayerThickNess(int layer)
{
	return 1.0;
}

float3 calcWorldSpacePos(int3 coord)
{
	uint3 imageDims;
	g_ResultImage.GetDimensions(imageDims.x, imageDims.y, imageDims.z);
	float2 uv = coord.xy / float2(imageDims.xy);
	
	float3 pos = lerp(g_PushConsts.frustumCornerTL, g_PushConsts.frustumCornerTR, uv.x);
	pos = lerp(pos, lerp(g_PushConsts.frustumCornerBL, g_PushConsts.frustumCornerBR, uv.x), uv.y);
	
	float d = coord.z / float(64);
	float n = 0.5;
	float f = 64.0;
	float z = n * exp2(d * (log2(f / n)));
	pos *= z / (f - n);
	
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
		const bool valid = all(abs(shadowCoord.xy) < 1.0);
		tc = valid ? shadowCoord : tc;
	}
	
	tc.xy = tc.xy * 0.5 + 0.5;
	const float shadow = tc.w != -1.0 ? g_ShadowImage.SampleCmpLevelZero(g_ShadowSampler, tc.xyw, tc.z) : 0.0;
	
	return g_PushConsts.sunLightRadiance * (1.0 - shadow);
}

float getPhaseFunction(float3 V, float3 L, float g)
{
	float num = -g * g + 1.0;
	float cosTheta = dot(L, V);
	float denom = 4.0 * PI * pow((g * g + 1.0) - 2.0 * g * cosTheta, 1.5);
	return num / denom;
}

float calcDensityFunction(float3 worldSpacePos)
{
	return g_PushConsts.density;
}

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	// world-space position of volumetric texture texel
	float3 worldSpacePos = calcWorldSpacePos(threadID);
	
	// thiccness of slice -- non-constant due to exponential slice distribution
	float layerThickness = computeLayerThickNess(threadID.z);
	
	// estimated density of participating medium at given point
	float density = calcDensityFunction(worldSpacePos);
	
	// scattering coefficient
	float scattering = g_PushConsts.scatteringCoefficient * density * layerThickness;
	
	// absorption coefficient
	float absorption = g_PushConsts.absorptionCoefficient * density * layerThickness;
	
	// normalized view dir
	float3 viewDirection = normalize(worldSpacePos - g_PushConsts.cameraPos.xyz);
	
	float3 lighting = 0.0;
	
	// lighting section:
	// adding all contributing lights radiance and multiplying it by
	// a phase function -- volumetric fog equivalent of BRDFs
	
	lighting += getSunLightingRadiance(worldSpacePos)
		* getPhaseFunction(-viewDirection, g_PushConsts.sunDirection.xyz, g_PushConsts.phaseAnisotropy);

	// finally, we apply some potentially non-white fog scattering albedo
	lighting *= unpackUnorm4x8(g_PushConsts.fogAlbedo).rgb;
	
	// final in-scattering is product of outgoing radiance and scattering
	// coefficients, while extinction is sum of scattering and absorption
	float4 finalOutValue = float4(lighting * scattering, density /*scattering + absorption*/);
	
	g_ResultImage[threadID] = finalOutValue;
}
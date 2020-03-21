#include "bindingHelper.hlsli"
#include "volumetricFogVBuffer.hlsli"

#define VOLUME_DEPTH (64)
#define VOLUME_NEAR (0.5)
#define VOLUME_FAR (64.0)

RWTexture3D<float4> g_ScatteringExtinctionImage : REGISTER_UAV(SCATTERING_EXTINCTION_IMAGE_BINDING, SCATTERING_EXTINCTION_IMAGE_SET);
RWTexture3D<float4> g_EmissivePhaseImage : REGISTER_UAV(EMISSIVE_PHASE_IMAGE_BINDING, EMISSIVE_PHASE_IMAGE_SET);

PUSH_CONSTS(PushConsts, g_PushConsts);

float3 calcWorldSpacePos(float3 texelCoord)
{
	uint3 imageDims;
	g_ScatteringExtinctionImage.GetDimensions(imageDims.x, imageDims.y, imageDims.z);
	float2 uv = texelCoord.xy / float2(imageDims.xy);
	
	float3 pos = lerp(g_PushConsts.frustumCornerTL, g_PushConsts.frustumCornerTR, uv.x);
	pos = lerp(pos, lerp(g_PushConsts.frustumCornerBL, g_PushConsts.frustumCornerBR, uv.x), uv.y);
	
	float d = texelCoord.z * (1.0 / VOLUME_DEPTH);
	float z = VOLUME_NEAR * exp2(d * (log2(VOLUME_FAR / VOLUME_NEAR)));
	pos *= z / VOLUME_FAR;
	
	pos += g_PushConsts.cameraPos;
	
	return pos;
}

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	const float3 worldSpacePos = calcWorldSpacePos(threadID + float3(g_PushConsts.jitterX, g_PushConsts.jitterY, g_PushConsts.jitterZ));
	g_ScatteringExtinctionImage[threadID] = g_PushConsts.scatteringExtinction;
	g_EmissivePhaseImage[threadID] = g_PushConsts.emissivePhase;
}
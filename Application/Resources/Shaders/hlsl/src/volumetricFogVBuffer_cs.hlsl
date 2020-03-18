#include "bindingHelper.hlsli"
#include "volumetricFogVBuffer.hlsli"

RWTexture3D<float4> g_ScatteringExtinctionImage : REGISTER_UAV(SCATTERING_EXTINCTION_IMAGE_BINDING, SCATTERING_EXTINCTION_IMAGE_SET);
RWTexture3D<float4> g_EmissivePhaseImage : REGISTER_UAV(EMISSIVE_PHASE_IMAGE_BINDING, EMISSIVE_PHASE_IMAGE_SET);

PUSH_CONSTS(PushConsts, g_PushConsts);


[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	g_ScatteringExtinctionImage[threadID] = g_PushConsts.scatteringExtinction;
	g_EmissivePhaseImage[threadID] = g_PushConsts.emissivePhase;
}
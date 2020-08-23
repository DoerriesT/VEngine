#include "bindingHelper.hlsli"
#include "common.hlsli"

RWTexture2D<float4> g_ResultImage : REGISTER_UAV(0, 0);

SamplerState g_Samplers[SAMPLER_COUNT] : REGISTER_SAMPLER(0, 1); // UNUSED

#include "commonAtmosphericScattering.hlsli"

ConstantBuffer<AtmosphereParameters> g_AtmosphereParams : REGISTER_CBV(1, 0);

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	AtmosphereParameters atmosphere = g_AtmosphereParams;
	
	g_ResultImage[threadID.xy] = float4(ComputeTransmittanceToTopAtmosphereBoundaryTexture(atmosphere, threadID.xy + 0.5) , 1.0);
}
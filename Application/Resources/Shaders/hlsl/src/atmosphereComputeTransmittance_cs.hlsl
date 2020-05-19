#include "bindingHelper.hlsli"

RWTexture2D<float4> g_ResultImage : REGISTER_UAV(0, 0);
SamplerState g_LinearSampler : REGISTER_SAMPLER(1, 0);

#include "commonAtmosphericScattering.hlsli"

ConstantBuffer<AtmosphereParameters> g_AtmosphereParams : REGISTER_CBV(2, 0);

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	AtmosphereParameters atmosphere = g_AtmosphereParams;
	
	g_ResultImage[threadID.xy] = float4(ComputeTransmittanceToTopAtmosphereBoundaryTexture(atmosphere, threadID.xy + 0.5) , 1.0);
}
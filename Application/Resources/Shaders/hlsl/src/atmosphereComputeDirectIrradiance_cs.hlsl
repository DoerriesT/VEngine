#include "bindingHelper.hlsli"
#include "common.hlsli"

RWTexture2D<float4> g_DeltaIrradianceImage : REGISTER_UAV(0, 0);
RWTexture2D<float4> g_IrradianceImage : REGISTER_UAV(1, 0);
Texture2D<float4> g_TransmittanceImage : REGISTER_SRV(2, 0);

SamplerState g_Samplers[SAMPLER_COUNT] : REGISTER_SAMPLER(0, 1);

#include "commonAtmosphericScattering.hlsli"

ConstantBuffer<AtmosphereParameters> g_AtmosphereParams : REGISTER_CBV(4, 0);

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	AtmosphereParameters atmosphere = g_AtmosphereParams;
	
	g_DeltaIrradianceImage[threadID.xy] = float4(ComputeDirectIrradianceTexture(atmosphere, g_TransmittanceImage, threadID.xy + 0.5), 1.0);
	g_IrradianceImage[threadID.xy] = 0.0;
}
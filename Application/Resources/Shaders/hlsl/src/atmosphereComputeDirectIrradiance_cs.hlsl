#include "bindingHelper.hlsli"

RWTexture2D<float4> g_DeltaIrradianceImage : REGISTER_UAV(0, 0);
RWTexture2D<float4> g_IrradianceImage : REGISTER_UAV(1, 0);
Texture2D<float4> g_TransmittanceImage : REGISTER_SRV(2, 0);
SamplerState g_LinearSampler : REGISTER_SAMPLER(3, 0);

#include "commonAtmosphericScattering.hlsli"

ConstantBuffer<AtmosphereParameters> g_AtmosphereParams : REGISTER_CBV(4, 0);

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	AtmosphereParameters atmosphere = g_AtmosphereParams;
	
	g_IrradianceImage[threadID.xy] = float4(ComputeDirectIrradianceTexture(atmosphere, g_TransmittanceImage, threadID.xy + 0.5), 1.0);
	g_IrradianceImage[threadID.xy] = 0.0;
}
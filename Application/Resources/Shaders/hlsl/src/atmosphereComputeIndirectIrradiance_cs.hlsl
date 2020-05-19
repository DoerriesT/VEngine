#include "bindingHelper.hlsli"

RWTexture2D<float4> g_DeltaIrradianceImage : REGISTER_UAV(0, 0);
RWTexture2D<float4> g_IrradianceImage : REGISTER_UAV(1, 0);
Texture3D<float4> g_SingleRayleighScatteringImage : REGISTER_SRV(2, 0);
Texture3D<float4> g_SingleMieScatteringImage : REGISTER_SRV(3, 0);
Texture3D<float4> g_MultipleScatteringImage : REGISTER_SRV(4, 0);
SamplerState g_LinearSampler : REGISTER_SAMPLER(5, 0);

struct PushConsts
{
	int scatteringOrder;
};

PUSH_CONSTS(PushConsts, g_PushConsts);

#include "commonAtmosphericScattering.hlsli"

ConstantBuffer<AtmosphereParameters> g_AtmosphereParams : REGISTER_CBV(6, 0);

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	AtmosphereParameters atmosphere = g_AtmosphereParams;
	
	float3 delta_irradiance = ComputeIndirectIrradianceTexture(
		atmosphere, g_SingleRayleighScatteringImage,
		g_SingleMieScatteringImage, g_MultipleScatteringImage,
		threadID.xy + 0.5, g_PushConsts.scatteringOrder);
	float3 irradiance = delta_irradiance;
	
	g_DeltaIrradianceImage[threadID.xy] = float4(delta_irradiance, 1.0);
	g_IrradianceImage[threadID.xy] = float4(irradiance, 1.0);
}
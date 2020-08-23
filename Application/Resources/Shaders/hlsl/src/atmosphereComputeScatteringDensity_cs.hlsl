#include "bindingHelper.hlsli"
#include "common.hlsli"

RWTexture3D<float4> g_ResultImage : REGISTER_UAV(0, 0);
Texture2D<float4> g_TransmittanceImage : REGISTER_SRV(1, 0);
Texture3D<float4> g_SingleRayleighScatteringImage : REGISTER_SRV(2, 0);
Texture3D<float4> g_SingleMieScatteringImage : REGISTER_SRV(3, 0);
Texture3D<float4> g_MultipleScatteringImage : REGISTER_SRV(4, 0);
Texture2D<float4> g_IrradianceImage : REGISTER_SRV(5, 0);

SamplerState g_Samplers[SAMPLER_COUNT] : REGISTER_SAMPLER(0, 1);

struct PushConsts
{
	int scatteringOrder;
};

PUSH_CONSTS(PushConsts, g_PushConsts);

#include "commonAtmosphericScattering.hlsli"

ConstantBuffer<AtmosphereParameters> g_AtmosphereParams : REGISTER_CBV(7, 0);

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	AtmosphereParameters atmosphere = g_AtmosphereParams;
	
	float3 scattering_density = ComputeScatteringDensityTexture(
			atmosphere, g_TransmittanceImage, g_SingleRayleighScatteringImage,
			g_SingleMieScatteringImage, g_MultipleScatteringImage,
			g_IrradianceImage, threadID + 0.5,
			g_PushConsts.scatteringOrder);
	
	g_ResultImage[threadID] = float4(scattering_density, 1.0);
}
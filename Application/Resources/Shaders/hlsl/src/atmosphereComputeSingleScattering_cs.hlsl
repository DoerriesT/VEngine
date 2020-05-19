#include "bindingHelper.hlsli"

RWTexture3D<float4> g_DeltaRayleighImage : REGISTER_UAV(0, 0);
RWTexture3D<float4> g_DeltaMieImage : REGISTER_UAV(1, 0);
RWTexture3D<float4> g_ScatteringImage : REGISTER_UAV(2, 0);
RWTexture3D<float4> g_SingleMieScatteringImage : REGISTER_UAV(3, 0);
Texture2D<float4> g_TransmittanceImage : REGISTER_SRV(4, 0);
SamplerState g_LinearSampler : REGISTER_SAMPLER(5, 0);

#include "commonAtmosphericScattering.hlsli"

ConstantBuffer<AtmosphereParameters> g_AtmosphereParams : REGISTER_CBV(6, 0);

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	AtmosphereParameters atmosphere = g_AtmosphereParams;
	
	float3 delta_rayleigh;
	float3 delta_mie;
	float4 scattering;
	float3 single_mie_scattering;
	
	ComputeSingleScatteringTexture(atmosphere, g_TransmittanceImage, threadID + 0.5, delta_rayleigh, delta_mie);
	scattering = float4(delta_rayleigh.rgb, delta_mie.r);
	single_mie_scattering = delta_mie;
	
	g_DeltaRayleighImage[threadID] = float4(delta_rayleigh, 1.0);
	g_DeltaMieImage[threadID] = float4(delta_mie, 1.0);
	g_ScatteringImage[threadID] = scattering;
	g_SingleMieScatteringImage[threadID] = float4(single_mie_scattering, 1.0);
}
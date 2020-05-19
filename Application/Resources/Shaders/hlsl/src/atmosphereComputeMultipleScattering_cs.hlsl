#include "bindingHelper.hlsli"

RWTexture3D<float4> g_DeltaMultipleScatteringImage : REGISTER_UAV(0, 0);
RWTexture3D<float4> g_ScatteringImage : REGISTER_UAV(1, 0);
Texture2D<float4> g_TransmittanceImage : REGISTER_SRV(2, 0);
Texture3D<float4> g_ScatteringDensityImage : REGISTER_SRV(3, 0);
SamplerState g_LinearSampler : REGISTER_SAMPLER(4, 0);

#include "commonAtmosphericScattering.hlsli"

ConstantBuffer<AtmosphereParameters> g_AtmosphereParams : REGISTER_CBV(5, 0);

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	AtmosphereParameters atmosphere = g_AtmosphereParams;
	
	float nu;
	float3 delta_multiple_scattering = ComputeMultipleScatteringTexture(atmosphere, g_TransmittanceImage, g_ScatteringDensityImage, threadID + 0.5, nu);
	float4 scattering = float4(delta_multiple_scattering.rgb / RayleighPhaseFunction(nu), 0.0);
	
	g_DeltaMultipleScatteringImage[threadID] = float4(delta_multiple_scattering, 1.0);
	g_ScatteringImage[threadID] += scattering;
}
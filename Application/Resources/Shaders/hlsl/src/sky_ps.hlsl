#include "bindingHelper.hlsli"
#include "common.hlsli"
#include "commonAtmosphericScatteringTypes.hlsli"

struct PSInput
{
	float4 ray : TEXCOORD;
};

struct PSOutput
{
	float4 color : SV_Target0;
	float4 normal : SV_Target1;
	float4 specularRoughness : SV_Target2;
};

TextureCubeArray<float4> g_SkyImage : REGISTER_SRV(0, 0);
SamplerState g_LinearSampler : REGISTER_SAMPLER(1, 0);
ConstantBuffer<AtmosphereParameters> g_AtmosphereParams : REGISTER_CBV(2, 0);
Texture2D<float4> g_TransmittanceImage : REGISTER_SRV(3, 0);
Texture3D<float4> g_ScatteringImage : REGISTER_SRV(4, 0);
ByteAddressBuffer g_ExposureData : REGISTER_SRV(5, 0);

struct PushConsts
{
	float4x4 invModelViewProjection;
	float3 sunDir;
	float cameraHeight;
};

PUSH_CONSTS(PushConsts, g_PushConsts);

#include "commonAtmosphericScattering.hlsli"

#define SKY_SPECTRAL_RADIANCE_TO_LUMINANCE float3(114974.91633649182, 71305.955740977355, 65310.549725826168)
#define SUN_SPECTRAL_RADIANCE_TO_LUMINANCE float3(98242.786221752205, 69954.398111511371, 66475.012354368271)
#define SUN_BRIGHTNESS 100.0

float3 GetSolarLuminance() 
{
	return g_AtmosphereParams.solar_irradiance /
		(PI * g_AtmosphereParams.sun_angular_radius * g_AtmosphereParams.sun_angular_radius) * SUN_BRIGHTNESS;// * SUN_SPECTRAL_RADIANCE_TO_LUMINANCE;
}
	
float3 GetSkyLuminance(float3 camera, float3 view_ray, float shadow_length, float3 sun_direction, out float3 transmittance) 
{
	return GetSkyRadiance(g_AtmosphereParams, g_TransmittanceImage,
		g_ScatteringImage, g_ScatteringImage /*unused*/,
		camera, view_ray, shadow_length, sun_direction, transmittance) * SUN_BRIGHTNESS;// * SKY_SPECTRAL_RADIANCE_TO_LUMINANCE;
}

[earlydepthstencil]
PSOutput main(PSInput input)
{
	PSOutput output = (PSOutput)0;
	
	float3 viewDir = normalize(input.ray.xyz);
	float3 transmittance = 0.0;
	float3 skyLuminance = GetSkyLuminance(float3(0.0, 6360000.0 + g_PushConsts.cameraHeight, 0.0), viewDir, 0.0, g_PushConsts.sunDir, transmittance);
	
	// If the view ray intersects the Sun, add the Sun radiance.
	if (dot(viewDir, g_PushConsts.sunDir) > cos(0.00935 / 2.0)) {
		skyLuminance = skyLuminance + transmittance * GetSolarLuminance();
	}
	
	output.color = float4(g_SkyImage.SampleLevel(g_LinearSampler, float4(input.ray.xyz, 0.0), 0.0).rgb, 1.0);//float4(float3(0.529, 0.808, 0.922), 1.0);
	output.color.rgb = transmittance.x == -5.0 ? output.color.rgb : skyLuminance;
	output.normal = 0.0;
	output.specularRoughness = 0.0;
	
	// apply pre-exposure
	output.color.rgb *= asfloat(g_ExposureData.Load(0));
	
	return output;
}
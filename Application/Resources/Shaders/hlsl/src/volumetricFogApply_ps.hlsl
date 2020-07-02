#include "bindingHelper.hlsli"
#include "volumetricFogApply2.hlsli"
#include "commonFilter.hlsli"
#include "srgb.hlsli"
#include "common.hlsli"
#include "commonEncoding.hlsli"

#define VOLUME_DEPTH (64)
#define VOLUME_NEAR (0.5)
#define VOLUME_FAR (64.0)

struct PSInput
{
	float4 position : SV_Position;
	float2 texCoord : TEXCOORD;
};

Texture2D<float4> g_DepthImage : REGISTER_SRV(DEPTH_IMAGE_BINDING, 0);
Texture3D<float4> g_VolumetricFogImage : REGISTER_SRV(VOLUMETRIC_FOG_IMAGE_BINDING, 0);
Texture2DArray<float4> g_BlueNoiseImage : REGISTER_SRV(BLUE_NOISE_IMAGE_BINDING, 0);
Texture2D<float4> g_RaymarchedVolumetricsImage : REGISTER_SRV(RAYMARCHED_VOLUMETRICS_IMAGE_BINDING, 0);

SamplerState g_LinearSampler : REGISTER_SAMPLER(LINEAR_SAMPLER_BINDING, 0);

PUSH_CONSTS(PushConsts, g_PushConsts);


[earlydepthstencil]
float4 main(PSInput input) : SV_Target0
{
	const float depth = g_DepthImage.Load(int3(input.position.xy, 0)).x;
	float z = 1.0 / (g_PushConsts.unprojectParams.z * depth + g_PushConsts.unprojectParams.w);
	float d = (log2(max(0, z * (1.0 / VOLUME_NEAR))) * (1.0 / log2(VOLUME_FAR / VOLUME_NEAR)));
	
	// the fog image can extend further to the right/downwards than the lighting image, so we cant just use the uv
	// of the current texel but instead need to scale the uv with respect to the fog image resolution
	float3 imageDims;
	g_VolumetricFogImage.GetDimensions(imageDims.x, imageDims.y, imageDims.z);
	float2 scaledFogImageTexelSize = 1.0 / (imageDims.xy * 8.0);
	
	float3 volumetricFogTexCoord = float3(input.position.xy * scaledFogImageTexelSize, d);
	
	float4 fog = 0.0;
	
	float4 noise = g_BlueNoiseImage.Load(int4((int2(input.position.xy) + 32 * (g_PushConsts.frame & 1)) & 63, g_PushConsts.frame & 63, 0));
	noise = (noise * 2.0 - 1.0) * 1.5;
	
	float4 result = float4(0.0, 0.0, 0.0, 1.0);
	
	if (g_PushConsts.raymarchedFog != 0)
	{
		float2 texelSize;
		g_RaymarchedVolumetricsImage.GetDimensions(texelSize.x, texelSize.y);
		texelSize = 1.0 / texelSize;
		
		float2 raymarchedTexCoord = input.position.xy * float2(g_PushConsts.texelWidth, g_PushConsts.texelHeight);
		float4 raymarchedVolumetrics = 0.0;
		float2 tc;
		
		tc = raymarchedTexCoord + noise.xy * texelSize;
		raymarchedVolumetrics += g_RaymarchedVolumetricsImage.SampleLevel(g_LinearSampler, tc, 0.0) / 4.0;
		noise = noise.yzwx;
		
		tc = raymarchedTexCoord + noise.xy * texelSize;
		raymarchedVolumetrics += g_RaymarchedVolumetricsImage.SampleLevel(g_LinearSampler, tc, 0.0) / 4.0;
		noise = noise.yzwx;
		
		tc = raymarchedTexCoord + noise.xy * texelSize;
		raymarchedVolumetrics += g_RaymarchedVolumetricsImage.SampleLevel(g_LinearSampler, tc, 0.0) / 4.0;
		noise = noise.yzwx;
		
		tc = raymarchedTexCoord + noise.xy * texelSize;
		raymarchedVolumetrics += g_RaymarchedVolumetricsImage.SampleLevel(g_LinearSampler, tc, 0.0) / 4.0;
		noise = noise.yzwx;
		
		result = raymarchedVolumetrics;
	}
	
	
	float3 texelSize = 1.0 / imageDims;
	
	float3 tc;
	
	tc = volumetricFogTexCoord + noise.xyz * texelSize;
	fog += g_VolumetricFogImage.SampleLevel(g_LinearSampler, tc, 0.0) / 4.0;
	noise = noise.yzwx;
	
	tc = volumetricFogTexCoord + noise.xyz * texelSize;
	fog += g_VolumetricFogImage.SampleLevel(g_LinearSampler, tc, 0.0) / 4.0;
	noise = noise.yzwx;
	
	tc = volumetricFogTexCoord + noise.xyz * texelSize;
	fog += g_VolumetricFogImage.SampleLevel(g_LinearSampler, tc, 0.0) / 4.0;
	noise = noise.yzwx;
	
	tc = volumetricFogTexCoord + noise.xyz * texelSize;
	fog += g_VolumetricFogImage.SampleLevel(g_LinearSampler, tc, 0.0) / 4.0;
	noise = noise.yzwx;
	
	fog.rgb = inverseSimpleTonemap(fog.rgb);
	
	result *= fog.a;
	result.rgb += fog.rgb;
	
	return result;
}
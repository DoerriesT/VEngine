#include "bindingHelper.hlsli"
#include "volumetricFogApply.hlsli"
#include "commonFilter.hlsli"

#define VOLUME_DEPTH (64)
#define VOLUME_NEAR (0.5)
#define VOLUME_FAR (64.0)

RWTexture2D<float4> g_ResultImage : REGISTER_UAV(RESULT_IMAGE_BINDING, RESULT_IMAGE_SET);
Texture2D<float4> g_DepthImage : REGISTER_SRV(DEPTH_IMAGE_BINDING, DEPTH_IMAGE_SET);
Texture3D<float4> g_VolumetricFogImage : REGISTER_SRV(VOLUMETRIC_FOG_IMAGE_BINDING, VOLUMETRIC_FOG_IMAGE_SET);
SamplerState g_LinearSampler : REGISTER_SAMPLER(LINEAR_SAMPLER_BINDING, LINEAR_SAMPLER_SET);

PUSH_CONSTS(PushConsts, g_PushConsts);

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	if (threadID.x >= g_PushConsts.width || threadID.y >= g_PushConsts.height)
	{
		return;
	}

	const float depth = g_DepthImage.Load(int3(threadID.xy, 0)).x;
	const float2 clipSpacePosition = (threadID.xy + 0.5) * float2(g_PushConsts.texelWidth, g_PushConsts.texelHeight) * 2.0 - 1.0;
	float4 viewSpacePosition4 = float4(g_PushConsts.unprojectParams.xy * clipSpacePosition, -1.0, g_PushConsts.unprojectParams.z * depth + g_PushConsts.unprojectParams.w);
	float3 viewSpacePosition = viewSpacePosition4.xyz / viewSpacePosition4.w;
	
	float3 result = g_ResultImage[threadID.xy].rgb;
	
	// volumetric fog
	{
		float z = -viewSpacePosition.z;
		float d = (log2(max(0, z * (1.0 / VOLUME_NEAR))) * (1.0 / log2(VOLUME_FAR / VOLUME_NEAR)));
		
		// the fog image can extend further to the right/downwards than the lighting image, so we cant just use the uv
		// of the current texel but instead need to scale the uv with respect to the fog image resolution
		float3 imageDims;
		g_VolumetricFogImage.GetDimensions(imageDims.x, imageDims.y, imageDims.z);
		float2 scaledFogImageTexelSize = 1.0 / (imageDims.xy * 8.0);
		
		float3 volumetricFogTexCoord = float3((threadID.xy + 0.5) * scaledFogImageTexelSize, d);
		
		float4 fog = sampleBicubic(g_VolumetricFogImage, g_LinearSampler, volumetricFogTexCoord.xy, imageDims.xy, 1.0 / imageDims.xy, d);
		//float4 fog = g_VolumetricFogImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], volumetricFogTexCoord, 0.0);
		
		fog.rgb = inverseSimpleTonemap(fog.rgb);
		result = result * fog.aaa + fog.rgb;
	}
	
	g_ResultImage[threadID.xy] = float4(result, 1.0);
}
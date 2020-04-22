#include "bindingHelper.hlsli"
#include "volumetricFogApply.hlsli"
#include "commonFilter.hlsli"
#include "srgb.hlsli"
#include "common.hlsli"
#include "commonEncoding.hlsli"

#define VOLUME_DEPTH (64)
#define VOLUME_NEAR (0.5)
#define VOLUME_FAR (64.0)

RWTexture2D<float4> g_ResultImage : REGISTER_UAV(RESULT_IMAGE_BINDING, RESULT_IMAGE_SET);
Texture2D<float4> g_DepthImage : REGISTER_SRV(DEPTH_IMAGE_BINDING, DEPTH_IMAGE_SET);
Texture3D<float4> g_VolumetricFogImage : REGISTER_SRV(VOLUMETRIC_FOG_IMAGE_BINDING, VOLUMETRIC_FOG_IMAGE_SET);
Texture2D<float4> g_IndirectSpecularLightImage : REGISTER_SRV(INDIRECT_SPECULAR_LIGHT_IMAGE_BINDING, INDIRECT_SPECULAR_LIGHT_IMAGE_SET);
Texture2D<float2> g_BrdfLutImage : REGISTER_SRV(BRDF_LUT_IMAGE_BINDING, BRDF_LUT_IMAGE_SET);
Texture2D<float4> g_SpecularRoughnessImage : REGISTER_SRV(SPEC_ROUGHNESS_IMAGE_BINDING, SPEC_ROUGHNESS_IMAGE_SET);
Texture2D<float2> g_NormalImage : REGISTER_SRV(NORMAL_IMAGE_BINDING, NORMAL_IMAGE_SET);
SamplerState g_LinearSampler : REGISTER_SAMPLER(LINEAR_SAMPLER_BINDING, LINEAR_SAMPLER_SET);

Texture2D<float4> g_Textures[TEXTURE_ARRAY_SIZE] : REGISTER_SRV(TEXTURES_BINDING, TEXTURES_SET);
SamplerState g_Samplers[SAMPLER_COUNT] : REGISTER_SAMPLER(SAMPLERS_BINDING, SAMPLERS_SET);

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
	
	// skip sky pixels
	if (depth == -10.0)
	{
		const float4 specularRoughness = approximateSRGBToLinear(g_SpecularRoughnessImage.Load(int3(threadID.xy, 0)));
		const float3 F0 = specularRoughness.xyz;
		const float roughness = max(specularRoughness.w, 0.04); // avoid precision problems
		const float3 V = -normalize(viewSpacePosition.xyz);
		const float3 N = decodeOctahedron(g_NormalImage.Load(int3(threadID.xy, 0)).xy);
		float4 indirectSpecular = g_IndirectSpecularLightImage.Load(int3(threadID.xy, 0));
		float2 brdfLut = g_BrdfLutImage.SampleLevel(g_LinearSampler, float2(roughness, saturate(dot(N, V))), 0.0).xy;
		indirectSpecular.rgb *= F0 * brdfLut.x + brdfLut.y;
		
		result += indirectSpecular.rgb * indirectSpecular.a;
	}
	
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
		
		float2 noiseTexCoord = float2(threadID.xy + 0.5) * g_PushConsts.noiseScale + (g_PushConsts.noiseJitter);
		float3 noise = g_Textures[g_PushConsts.noiseTexId].SampleLevel(g_Samplers[SAMPLER_POINT_REPEAT], noiseTexCoord, 0.0).xyz;
		
		if (depth == -1.0)
		{
			volumetricFogTexCoord += (noise * 2.0 - 1.0) * 1.5 / imageDims;
		}
		
		float4 fog = sampleBicubic(g_VolumetricFogImage, g_LinearSampler, volumetricFogTexCoord.xy, imageDims.xy, 1.0 / imageDims.xy, d);
		//float4 fog = g_VolumetricFogImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], volumetricFogTexCoord, 0.0);
		
		fog.rgb = inverseSimpleTonemap(fog.rgb);
		result = result * fog.aaa + fog.rgb;
	}
	
	g_ResultImage[threadID.xy] = float4(result, 1.0);
}
#include "bindingHelper.hlsli"
#include "lightProbeGBuffer.hlsli"
#include "srgb.hlsli"
#include "commonEncoding.hlsli"
#include "common.hlsli"
#include "brdf.hlsli"

RWTexture2DArray<float4> g_ResultImage : REGISTER_UAV(RESULT_IMAGE_BINDING, 0);
Texture2DArray<float> g_DepthImage : REGISTER_SRV(DEPTH_IMAGE_BINDING, 0);
Texture2DArray<float4> g_AlbedoRoughnessImage : REGISTER_SRV(ALBEDO_ROUGHNESS_IMAGE_BINDING, 0);
Texture2DArray<float2> g_NormalImage : REGISTER_SRV(NORMAL_IMAGE_BINDING, 0);
ConstantBuffer<Constants> g_Constants : REGISTER_CBV(CONSTANT_BUFFER_BINDING, 0);
Texture2DArray<float> g_ShadowImage : REGISTER_SRV(DIRECTIONAL_LIGHTS_SHADOW_IMAGE_BINDING, 0);
StructuredBuffer<float4x4> g_ShadowMatrices : REGISTER_SRV(SHADOW_MATRICES_BINDING, 0);

// directional lights
StructuredBuffer<DirectionalLight> g_DirectionalLights : REGISTER_SRV(DIRECTIONAL_LIGHTS_BINDING, 0);
StructuredBuffer<DirectionalLight> g_DirectionalLightsShadowed : REGISTER_SRV(DIRECTIONAL_LIGHTS_SHADOWED_BINDING, 0);

SamplerComparisonState g_ShadowSampler : REGISTER_SAMPLER(0, 1);

//PUSH_CONSTS(PushConsts, g_PushConsts);


[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID, uint3 groupID : SV_GroupID)
{
	if (threadID.x >= g_Constants.width || threadID.y >= g_Constants.width)
	{
		return;
	}

	float depth = g_DepthImage.Load(int4(threadID.x, threadID.y, threadID.z + g_Constants.arrayTextureOffset, 0)).x;
	
	if (depth == 1.0)
	{
		g_ResultImage[threadID] = float4(0.529, 0.808, 0.922, 1.0);
		return;
	}
	
	float4 viewSpacePos4 = mul(g_Constants.probeFaceToViewSpace[groupID.z], float4((threadID.xy + 0.5) * g_Constants.texelSize * float2(2.0, -2.0) - float2(1.0, -1.0), depth, 1.0));
	float3 viewSpacePos = viewSpacePos4.xyz / viewSpacePos4.w;
	float4 albedoRoughness = accurateSRGBToLinear(g_AlbedoRoughnessImage.Load(int4(threadID.x, threadID.y, threadID.z + g_Constants.arrayTextureOffset, 0)));
	float3 albedo = albedoRoughness.rgb;
	float roughness = albedoRoughness.a;
	float3 N = mul(g_Constants.viewMatrix, float4(decodeOctahedron(g_NormalImage.Load(int4(threadID.x, threadID.y, threadID.z + g_Constants.arrayTextureOffset, 0)).xy), 0.0)).xyz;
	
	float3 result = 0.0;
	
	result += (1.0 / PI) * albedo;

	// directional lights
	{
		for (uint i = 0; i < g_Constants.directionalLightCount; ++i)
		{
			DirectionalLight light = g_DirectionalLights[i];
			result += Diffuse_Lambert(albedo) * light.color * saturate(dot(N, light.direction));
		}
	}
	
	const float3 worldSpacePos = mul(g_Constants.invViewMatrix, float4(viewSpacePos, 1.0)).xyz;
	// shadowed directional lights
	{
		
		for (uint i = 0; i < g_Constants.directionalLightShadowedCount; ++i)
		{
			DirectionalLight light = g_DirectionalLightsShadowed[i];
			float4x4 shadowMatrix;
			const float4 tc = float4(mul(g_ShadowMatrices[light.shadowOffset], float4(worldSpacePos.xyz, 1.0)).xyz, i);
			float shadow = g_ShadowImage.SampleCmpLevelZero(g_ShadowSampler, float3(tc.xy * float2(0.5, -0.5) + 0.5, tc.w), tc.z - 0.001).x ;
			result += Diffuse_Lambert(albedo) * light.color * (saturate(dot(N, light.direction)) * shadow);
		}
	}
	
	g_ResultImage[threadID] = float4(result, 1.0);
}
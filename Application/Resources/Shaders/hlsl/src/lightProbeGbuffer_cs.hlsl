#include "bindingHelper.hlsli"
#include "lightProbeGBuffer.hlsli"
#include "srgb.hlsli"
#include "commonEncoding.hlsli"
#include "common.hlsli"
#include "brdf.hlsli"

RWTexture2D<float4> g_ResultImage[6] : REGISTER_UAV(RESULT_IMAGE_BINDING, RESULT_IMAGE_SET);
Texture2D<float> g_DepthImage[6] : REGISTER_SRV(DEPTH_IMAGE_BINDING, DEPTH_IMAGE_SET);
Texture2D<float4> g_AlbedoRoughnessImage[6] : REGISTER_SRV(ALBEDO_ROUGHNESS_IMAGE_BINDING, ALBEDO_ROUGHNESS_IMAGE_SET);
Texture2D<float2> g_NormalImage[6] : REGISTER_SRV(NORMAL_IMAGE_BINDING, NORMAL_IMAGE_SET);
ConstantBuffer<Constants> g_Constants : REGISTER_CBV(CONSTANT_BUFFER_BINDING, CONSTANT_BUFFER_SET);

// directional lights
StructuredBuffer<DirectionalLight> g_DirectionalLights : REGISTER_SRV(DIRECTIONAL_LIGHTS_BINDING, DIRECTIONAL_LIGHTS_SET);
StructuredBuffer<DirectionalLight> g_DirectionalLightsShadowed : REGISTER_SRV(DIRECTIONAL_LIGHTS_SHADOWED_BINDING, DIRECTIONAL_LIGHTS_SHADOWED_SET);


//PUSH_CONSTS(PushConsts, g_PushConsts);


[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID, uint3 groupID : SV_GroupID)
{
	if (threadID.x >= g_Constants.width || threadID.y >= g_Constants.height)
	{
		return;
	}

	float depth = g_DepthImage[groupID.z].Load(int3(threadID.xy, 0)).x;
	
	if (depth == 1.0)
	{
		g_ResultImage[groupID.z][threadID.xy] = float4(0.529, 0.808, 0.922, 1.0);
		return;
	}
	
	float4 viewSpacePos4 = mul(g_Constants.probeFaceToViewSpace[groupID.z], float4((threadID.xy + 0.5) * g_Constants.texelSize * 2.0 - 1.0, depth, 1.0));
	float3 viewSpacePos = viewSpacePos4.xyz / viewSpacePos4.w;
	float4 albedoRoughness = accurateSRGBToLinear(g_AlbedoRoughnessImage[groupID.z].Load(int3(threadID.xy, 0)));
	float3 albedo = albedoRoughness.rgb;
	float roughness = albedoRoughness.a;
	float3 N = mul(g_Constants.viewMatrix, float4(decodeOctahedron(g_NormalImage[groupID.z].Load(int3(threadID.xy, 0)).xy), 0.0)).xyz;
	
	float3 result = 0.0;
	
	result += 1.0 * albedo;

	// directional lights
	{
		for (uint i = 0; i < g_Constants.directionalLightCount; ++i)
		{
			DirectionalLight light = g_DirectionalLights[i];
			result += Diffuse_Lambert(albedo) * light.color * saturate(dot(N, light.direction));
		}
	}
	
	// shadowed directional lights
	{
		for (uint i = 0; i < g_Constants.directionalLightShadowedCount; ++i)
		{
			DirectionalLight light = g_DirectionalLightsShadowed[i];
			result += Diffuse_Lambert(albedo) * light.color * saturate(dot(N, light.direction));
		}
	}
	
	g_ResultImage[groupID.z][threadID.xy] = float4(result, 1.0);
}
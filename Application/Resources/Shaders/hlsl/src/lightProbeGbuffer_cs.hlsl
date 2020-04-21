#include "bindingHelper.hlsli"
#include "lightProbeGBuffer.hlsli"
#include "srgb.hlsli"
#include "commonEncoding.hlsli"
#include "lighting.hlsl"

RWTexture2D<float4> g_ResultImage[6] : REGISTER_UAV(RESULT_IMAGE_BINDING, RESULT_IMAGE_SET);
Texture2D<float> g_DepthImage[6] : REGISTER_SRV(DEPTH_IMAGE_BINDING, DEPTH_IMAGE_SET);
Texture2D<float4> g_AlbedoRoughnessImage[6] : REGISTER_SRV(ALBEDO_ROUGHNESS_IMAGE_BINDING, ALBEDO_ROUGHNESS_IMAGE_SET);
Texture2D<float2> g_NormalImage[6] : REGISTER_SRV(NORMAL_IMAGE_BINDING, NORMAL_IMAGE_SET);
ConstantBuffer<Constants> g_Constants : REGISTER_CBV(CONSTANT_BUFFER_BINDING, CONSTANT_BUFFER_SET);

RWTexture2D<float> g_ResultImage : REGISTER_UAV(RESULT_IMAGE_BINDING, RESULT_IMAGE_SET);
Texture2D<float> g_DepthImage : REGISTER_SRV(DEPTH_IMAGE_BINDING, DEPTH_IMAGE_SET);
Texture2DArray<float> g_ShadowImage : REGISTER_SRV(SHADOW_IMAGE_BINDING, SHADOW_IMAGE_SET);
SamplerComparisonState g_ShadowSampler : REGISTER_SAMPLER(SHADOW_SAMPLER_BINDING, SHADOW_SAMPLER_SET);
SamplerState g_PointSampler : REGISTER_SAMPLER(POINT_SAMPLER_BINDING, POINT_SAMPLER_SET);
StructuredBuffer<float4x4> g_ShadowMatrices : REGISTER_SRV(SHADOW_MATRICES_BINDING, SHADOW_MATRICES_SET);
StructuredBuffer<float4> g_CascadeParams : REGISTER_SRV(CASCADE_PARAMS_BUFFER_BINDING, CASCADE_PARAMS_BUFFER_SET);  // X: depth bias Y: normal bias Z: texelsPerMeter


//PUSH_CONSTS(PushConsts, g_PushConsts);


[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID, uint3 groupID : SV_GroupID)
{
	if (threadID.x >= g_Constants.width || threadID.y >= g_Constants.height)
	{
		return;
	}

	float depth = g_DepthImage[groupID.z].Load(int3(threadID.xy, 0)).x;
	
	if (depth == 0.0)
	{
		g_ResultImage[groupID.z][threadID.xy] = float4(0.529, 0.808, 0.922, 1.0);
		return;
	}
	
	float4 viewSpacePos4 = mul(g_Constants.probeFaceToViewSpace[groupID.z], float4((threadID.xy + 0.5) * g_Constants.texelSize * 2.0 - 1.0, depth, 1.0));
	float3 viewSpacePos = viewSpacePos4.xyz / viewSpacePos4.w;
	float3 V = -normalize(viewSpacePos);
	float4 albedoRoughness = accurateSRGBToLinear(g_AlbedoRoughnessImage[groupID.z].Load(int3(threadID.xy, 0)));
	float3 albedo = albedoRoughness.rgb;
	float roughness = albedoRoughness.a;
	float3 N = decodeOctahedron(g_NormalImage[groupID.z].Load(int3(threadID.xy, 0)).xy);
	
	float3 result = 0.0;
	
	result += 1.0 * albedo;
	
	result += Diffuse_Lit(albedo, float3 radiance, N, V, float3 L, roughness);
	
	g_ResultImage[groupID.z][threadID.xy] = float4(result, 1.0);
}
#include "bindingHelper.hlsli"

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

[earlydepthstencil]
PSOutput main(PSInput input)
{
	PSOutput output = (PSOutput)0;
	
	output.color = float4(g_SkyImage.SampleLevel(g_LinearSampler, float4(input.ray.xyz, 0.0), 0.0).rgb, 1.0);//float4(float3(0.529, 0.808, 0.922), 1.0);
	output.normal = 0.0;
	output.specularRoughness = 0.0;
	
	return output;
}
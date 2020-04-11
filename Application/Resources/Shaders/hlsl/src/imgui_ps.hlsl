#include "bindingHelper.hlsli"

struct PSInput
{
	float4 position : SV_Position;
	float4 color : COLOR0;
	float2 texCoord : TEXCOORD0;
};

Texture2D<float4> g_Texture : REGISTER_SRV(0, 0);
SamplerState g_Sampler : REGISTER_SAMPLER(1, 0);

float4 main(PSInput input) : SV_Target0
{
	return input.color * g_Texture.Sample(g_Sampler, input.texCoord);
}
#include "bindingHelper.hlsli"
#include "common.hlsli"

struct PSInput
{
	float4 position : SV_Position;
	float4 color : COLOR0;
	float2 texCoord : TEXCOORD0;
	nointerpolation uint textureIndex : TEXTURE_INDEX;
};

Texture2D<float4> g_Textures[TEXTURE_ARRAY_SIZE] : REGISTER_SRV(0, 0);
SamplerState g_Samplers[SAMPLER_COUNT] : REGISTER_SAMPLER(1, 0);

float4 main(PSInput input) : SV_Target0
{
	return input.color * g_Textures[input.textureIndex].Sample(g_Samplers[SAMPLER_LINEAR_REPEAT], input.texCoord);
}
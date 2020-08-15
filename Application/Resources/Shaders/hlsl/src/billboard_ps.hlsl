#include "bindingHelper.hlsli"
#include "common.hlsli"
#include "srgb.hlsli"

struct PSInput
{
	float4 position : SV_Position;
	float2 texCoord : TEXCOORD;
	float opacity : OPACITY;
	nointerpolation uint textureIndex : TEXTURE_INDEX;
};

struct PSOutput
{
	float4 color : SV_Target0;
};

Texture2D<float4> g_Textures[TEXTURE_ARRAY_SIZE] : REGISTER_SRV(0, 1);
SamplerState g_Samplers[SAMPLER_COUNT] : REGISTER_SAMPLER(1, 1);

float4 main(PSInput input) : SV_Target0
{
	float4 albedoOpacity = 1.0;
	if (input.textureIndex != 0)
	{
		albedoOpacity = g_Textures[input.textureIndex - 1].Sample(g_Samplers[SAMPLER_LINEAR_CLAMP], input.texCoord);
		albedoOpacity.rgb = accurateSRGBToLinear(albedoOpacity.rgb);
	}
	albedoOpacity.a *= input.opacity;
	
	return albedoOpacity;
}
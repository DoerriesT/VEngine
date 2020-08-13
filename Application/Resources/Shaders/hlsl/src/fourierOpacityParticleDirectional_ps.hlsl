#include "bindingHelper.hlsli"
#include "fourierOpacityParticleDirectional.hlsli"
#include "common.hlsli"
#include "commonEncoding.hlsli"
#include "commonFourierOpacity.hlsli"

Texture2D<float4> g_Textures[TEXTURE_ARRAY_SIZE] : REGISTER_SRV(0, 1);
SamplerState g_Samplers[SAMPLER_COUNT] : REGISTER_SAMPLER(1, 1);

struct PSInput
{
	float4 position : SV_Position;
	float2 texCoord : TEXCOORD;
	nointerpolation float opacity : OPACITY;
	nointerpolation uint textureIndex : TEXTURE_INDEX;
};

struct PSOutput
{
	float4 fom0 : SV_Target0;
	float4 fom1 : SV_Target1;
};

PSOutput main(PSInput input)
{
	float opacity = input.opacity * 0.99;
	if (input.textureIndex != 0)
	{
		opacity *= g_Textures[input.textureIndex - 1].Sample(g_Samplers[SAMPLER_LINEAR_REPEAT], input.texCoord).a;
	}
	
	float4 result0 = 0.0;
	float4 result1 = 0.0;

	
	fourierOpacityAccumulate(input.position.z, 1.0 - opacity, result0, result1);
	
	
	PSOutput output;
	output.fom0 = result0;
	output.fom1 = result1;
	
	return output;
}
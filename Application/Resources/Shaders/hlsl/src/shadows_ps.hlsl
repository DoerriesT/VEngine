#include "bindingHelper.hlsli"
#include "shadows.hlsli"
#include "common.hlsli"

struct PSInput
{
	float4 position : SV_Position;
	float2 texCoord : TEXCOORD;
	nointerpolation uint textureIndex : TEXTURE_INDEX;
};

Texture2D<float4> g_Textures[TEXTURE_ARRAY_SIZE] : REGISTER_SRV(0, 1);
SamplerState g_Samplers[SAMPLER_COUNT] : REGISTER_SAMPLER(0, 2);

void main(PSInput input)
{
	if (input.textureIndex != 0)
	{
		Texture2D<float4> tex = g_Textures[NonUniformResourceIndex(input.textureIndex - 1)];
		SamplerState sampler = g_Samplers[SAMPLER_LINEAR_REPEAT];
		float alpha = tex.Sample(sampler, input.texCoord).a;
		
		float2 texSize;
		float mipLevels;
		tex.GetDimensions(0, texSize.x, texSize.y, mipLevels);
		
		float2 p = max(ddx(input.texCoord * texSize), ddy(input.texCoord * texSize));
		float lod = clamp(log2(max(p.x, p.y)), 0.0, mipLevels - 1.0);
		
		alpha *= 1.0 + lod * ALPHA_MIP_SCALE;
		
		if (alpha < ALPHA_CUTOFF)
		{
			discard;
		}
	}
}
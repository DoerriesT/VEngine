#include "bindingHelper.hlsli"
#include "visibilityBuffer.hlsli"
#include "common.hlsli"
#include "packing.hlsli"

#ifndef ALPHA_MASK_ENABLED
#define ALPHA_MASK_ENABLED 0
#endif // ALPHA_MASK_ENABLED

struct PSInput
{
	float4 position : SV_Position;
	nointerpolation uint instanceID : INSTANCE_ID;
#if ALPHA_MASK_ENABLED
	float2 texCoord : TEXCOORD;
	nointerpolation uint textureIndex : TEXTURE_INDEX;
#endif // ALPHA_MASK_ENABLED
	uint primitiveID : SV_PrimitiveID;
};

#if ALPHA_MASK_ENABLED
Texture2D<float4> g_Textures[TEXTURE_ARRAY_SIZE] : REGISTER_SRV(TEXTURES_BINDING, TEXTURES_SET);
SamplerState g_Samplers[SAMPLER_COUNT] : REGISTER_SAMPLER(SAMPLERS_BINDING, SAMPLERS_SET);
#endif // ALPHA_MASK_ENABLED

#if !ALPHA_MASK_ENABLED
[earlydepthstencil]
#endif // ALPHA_MASK_ENABLED
float4 main(PSInput input) : SV_Target0
{
#if ALPHA_MASK_ENABLED
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
#endif // ALPHA_MASK_ENABLED
	
	uint triangleData = (input.instanceID & 0xFFFF) << 16;
	triangleData |= input.primitiveID & 0xFFFF;
	
	return unpackUnorm4x8(triangleData);
}
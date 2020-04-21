#include "bindingHelper.hlsli"
#include "probeGBuffer.hlsli"
#include "lighting.hlsli"
#include "common.hlsli"
#include "commonEncoding.hlsli"
#include "packing.hlsli"

#ifndef ALPHA_MASK_ENABLED
#define ALPHA_MASK_ENABLED 0
#endif // ALPHA_MASK_ENABLED


struct PSInput
{
	float4 position : SV_Position;
	float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
	float4 worldPos : WORLD_POSITION;
	nointerpolation uint materialIndex : MATERIAL_INDEX;
};

struct PSOutput
{
	float4 albedoRoughness : SV_Target0;
	float4 normal : SV_Target1;
};

StructuredBuffer<MaterialData> g_MaterialData : REGISTER_SRV(MATERIAL_DATA_BINDING, MATERIAL_DATA_SET);

Texture2D<float4> g_Textures[TEXTURE_ARRAY_SIZE] : REGISTER_SRV(TEXTURES_BINDING, TEXTURES_SET);
SamplerState g_Samplers[SAMPLER_COUNT] : REGISTER_SAMPLER(SAMPLERS_BINDING, SAMPLERS_SET);


#if !ALPHA_MASK_ENABLED
[earlydepthstencil]
#endif // !ALPHA_MASK_ENABLED
PSOutput main(PSInput input)
{
	PSOutput output = (PSOutput)0;
	
	const MaterialData materialData = g_MaterialData[input.materialIndex];
	
	// albedo
	{
		float3 albedo = unpackUnorm4x8(materialData.albedoOpacity).rgb;
		uint albedoTextureIndex = (materialData.albedoNormalTexture & 0xFFFF0000) >> 16;
		if (albedoTextureIndex != 0)
		{
#if !ALPHA_MASK_ENABLED
			albedo = g_Textures[NonUniformResourceIndex(albedoTextureIndex - 1)].Sample(g_Samplers[SAMPLER_LINEAR_REPEAT], input.texCoord).rgb;
#else
			float4 tap = g_Textures[NonUniformResourceIndex(albedoTextureIndex - 1)].Sample(g_Samplers[SAMPLER_LINEAR_REPEAT], input.texCoord);
			albedo = tap.rgb;
			float alpha = tap.a;
		
			float2 texSize;
			float mipLevels;
			g_Textures[NonUniformResourceIndex(albedoTextureIndex - 1)].GetDimensions(0, texSize.x, texSize.y, mipLevels);
			
			float2 p = max(ddx(input.texCoord * texSize), ddy(input.texCoord * texSize));
			float lod = clamp(log2(max(p.x, p.y)), 0.0, mipLevels - 1.0);
			
			alpha *= 1.0 + lod * ALPHA_MIP_SCALE;
			
			if (alpha < ALPHA_CUTOFF)
			{
				discard;
			}
#endif // !ALPHA_MASK_ENABLED
		}
		output.albedoRoughness.rgb = albedo;
	}
	
	// roughness
	{
		float roughness = unpackUnorm4x8(materialData.metalnessRoughness).y;
		uint roughnessTextureIndex = (materialData.metalnessRoughnessTexture & 0xFFFF);
		if (roughnessTextureIndex != 0)
		{
			roughness = g_Textures[NonUniformResourceIndex(roughnessTextureIndex - 1)].Sample(g_Samplers[SAMPLER_LINEAR_REPEAT], input.texCoord).y;
		}
		output.albedoRoughness.a = roughness;
	}
	
	// normal
	{
		float3 normal = normalize(input.normal);
		uint normalTextureIndex = (materialData.albedoNormalTexture & 0xFFFF);
		if (normalTextureIndex != 0)
		{
			float3 tangentSpaceNormal;
			tangentSpaceNormal.xy = g_Textures[NonUniformResourceIndex(normalTextureIndex - 1)].Sample(g_Samplers[SAMPLER_LINEAR_REPEAT], input.texCoord).xy * 2.0 - 1.0;
			tangentSpaceNormal.z = sqrt(1.0 - tangentSpaceNormal.x * tangentSpaceNormal.x + tangentSpaceNormal.y * tangentSpaceNormal.y);
			normal = mul(tangentSpaceNormal, calculateTBN(normal, input.worldPos.xyz / input.worldPos.w, float2(input.texCoord.x, -input.texCoord.y)));
			normal = normalize(normal);
		}
		output.normal.xy = encodeOctahedron(normal);
	}
	
	return output;
}
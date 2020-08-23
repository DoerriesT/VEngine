#include "bindingHelper.hlsli"
#include "depthPrepass.hlsli"
#include "common.hlsli"
#include "commonVertex.hlsli"

#ifndef ALPHA_MASK_ENABLED
#define ALPHA_MASK_ENABLED 0
#endif // ALPHA_MASK_ENABLED

struct VSOutput
{
	float4 position : SV_Position;
#if ALPHA_MASK_ENABLED
	float2 texCoord : TEXCOORD;
	nointerpolation uint textureIndex : TEXTURE_INDEX;
#endif // ALPHA_MASK_ENABLED
};

StructuredBuffer<float4> g_TransformData : REGISTER_SRV(TRANSFORM_DATA_BINDING, 0);
StructuredBuffer<uint> g_Positions : REGISTER_SRV(VERTEX_POSITIONS_BINDING, 0);
#if ALPHA_MASK_ENABLED
StructuredBuffer<uint> g_TexCoords : REGISTER_SRV(VERTEX_TEXCOORDS_BINDING, 0);
StructuredBuffer<MaterialData> g_MaterialData : REGISTER_SRV(MATERIAL_DATA_BINDING, 0);
#endif // ALPHA_MASK_ENABLED

PUSH_CONSTS(PushConsts, g_PushConsts);

VSOutput main(uint vertexID : SV_VertexID) 
{
	VSOutput output;
	
	const float3 pos = getPosition(vertexID, g_Positions);
	float3 worldPos;
	worldPos.x = dot(g_TransformData[g_PushConsts.transformIndex * 4 + 0], float4(pos, 1.0));
	worldPos.y = dot(g_TransformData[g_PushConsts.transformIndex * 4 + 1], float4(pos, 1.0));
	worldPos.z = dot(g_TransformData[g_PushConsts.transformIndex * 4 + 2], float4(pos, 1.0));
	
	output.position = mul(g_PushConsts.jitteredViewProjectionMatrix, float4(worldPos, 1.0));

#if ALPHA_MASK_ENABLED
	output.texCoord = getTexCoord(vertexID, g_TexCoords) * g_PushConsts.texCoordScale + g_PushConsts.texCoordBias;
	output.textureIndex = g_MaterialData[g_PushConsts.materialIndex].albedoNormalTexture >> 16;
#endif // ALPHA_MASK_ENABLED
	
	return output;
}
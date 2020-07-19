#include "bindingHelper.hlsli"
#include "visibilityBuffer.hlsli"
#include "common.hlsli"

#ifndef ALPHA_MASK_ENABLED
#define ALPHA_MASK_ENABLED 0
#endif // ALPHA_MASK_ENABLED

struct VSOutput
{
	float4 position : SV_Position;
	nointerpolation uint instanceID : INSTANCE_ID;
#if ALPHA_MASK_ENABLED
	float2 texCoord : TEXCOORD;
	nointerpolation uint textureIndex : TEXTURE_INDEX;
#endif // ALPHA_MASK_ENABLED
};

StructuredBuffer<float4x4> g_TransformMatrices : REGISTER_SRV(TRANSFORM_DATA_BINDING, TRANSFORM_DATA_SET);
StructuredBuffer<float> g_Positions : REGISTER_SRV(VERTEX_POSITIONS_BINDING, VERTEX_POSITIONS_SET);
#if ALPHA_MASK_ENABLED
StructuredBuffer<float> g_TexCoords : REGISTER_SRV(VERTEX_TEXCOORDS_BINDING, VERTEX_TEXCOORDS_SET);
StructuredBuffer<MaterialData> g_MaterialData : REGISTER_SRV(MATERIAL_DATA_BINDING, MATERIAL_DATA_SET);
#endif // ALPHA_MASK_ENABLED

PUSH_CONSTS(PushConsts, g_PushConsts);

VSOutput main(uint vertexID : SV_VertexID) 
{
	VSOutput output;
	
	const float4x4 modelMatrix = g_TransformMatrices[g_PushConsts.transformIndex];
	const float3 position = float3(g_Positions[vertexID * 3 + 0], g_Positions[vertexID * 3 + 1], g_Positions[vertexID * 3 + 2]);									
	
	output.position = mul(g_PushConsts.jitteredViewProjectionMatrix, mul(modelMatrix, float4(position, 1.0)));
	output.instanceID = g_PushConsts.instanceID;

#if ALPHA_MASK_ENABLED
	output.texCoord = float2(g_TexCoords[vertexID * 2 + 0], g_TexCoords[vertexID * 2 + 1]);
	output.textureIndex = g_MaterialData[g_PushConsts.materialIndex].albedoNormalTexture >> 16;
#endif // ALPHA_MASK_ENABLED
	
	return output;
}
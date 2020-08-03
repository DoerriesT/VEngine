#include "bindingHelper.hlsli"
#include "probeGBuffer.hlsli"
#include "packing.hlsli"
#include "commonVertex.hlsli"

struct VSOutput
{
	float4 position : SV_Position;
	float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
	float4 worldPos : WORLD_POSITION;
	nointerpolation uint materialIndex : MATERIAL_INDEX;
};

ConstantBuffer<Constants> g_Constants : REGISTER_CBV(CONSTANT_BUFFER_BINDING, 0);
StructuredBuffer<float4> g_TransformData : REGISTER_SRV(TRANSFORM_DATA_BINDING, 0);
StructuredBuffer<uint> g_Positions : REGISTER_SRV(VERTEX_POSITIONS_BINDING, 0);
StructuredBuffer<uint> g_QTangents : REGISTER_SRV(VERTEX_QTANGENTS_BINDING, 0);
StructuredBuffer<uint> g_TexCoords : REGISTER_SRV(VERTEX_TEXCOORDS_BINDING, 0);

PUSH_CONSTS(PushConsts, g_PushConsts);

VSOutput main(uint vertexID : SV_VertexID) 
{
	VSOutput output = (VSOutput)0;
	
	const float3 pos = getPosition(vertexID, g_Positions);
	float3 worldPos;
	worldPos.x = dot(g_TransformData[g_PushConsts.transformIndex * 4 + 0], float4(pos, 1.0));
	worldPos.y = dot(g_TransformData[g_PushConsts.transformIndex * 4 + 1], float4(pos, 1.0));
	worldPos.z = dot(g_TransformData[g_PushConsts.transformIndex * 4 + 2], float4(pos, 1.0));
	
	output.position = mul(g_Constants.viewProjectionMatrix[g_PushConsts.face], float4(worldPos, 1.0));
	output.worldPos = float4(worldPos, 1.0);
	
	output.texCoord = getTexCoord(vertexID, g_TexCoords) * g_PushConsts.texCoordScale + g_PushConsts.texCoordBias;
	
	float4 qtangent = getQTangent(vertexID, g_QTangents);
	float4 tangent;
	tangent.w = qtangent.w < 0.0 ? -1.0 : 1.0;
	
	// rotate to world space
	qtangent = quaternionMult(g_TransformData[g_PushConsts.transformIndex * 4 + 3], qtangent);
	
	// extract normal and tangent
	quaternionToNormalTangent(qtangent, output.normal, tangent.xyz);
	
	output.materialIndex = g_PushConsts.materialIndex;
	
	return output;
}
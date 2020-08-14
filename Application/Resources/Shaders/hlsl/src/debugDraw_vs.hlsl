#include "bindingHelper.hlsli"

struct VSOutput
{
	float4 position : SV_Position;
	float4 color : COLOR;
};

struct DebugVertex
{
	float4 position;
	float4 color;
};

struct PushConsts
{
	float4x4 viewProjectionMatrix;
};

StructuredBuffer<DebugVertex> g_Vertices : REGISTER_SRV(0, 0);

PUSH_CONSTS(PushConsts, g_PushConsts);


VSOutput main(uint vertexID : SV_VertexID) 
{
	DebugVertex vertex = g_Vertices[vertexID];
	
	VSOutput output;
	
	output.position = mul(g_PushConsts.viewProjectionMatrix, float4(vertex.position.xyz, 1.0));
	output.color = vertex.color;
	
	return output;
}
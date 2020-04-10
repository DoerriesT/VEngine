#include "bindingHelper.hlsli"

struct VSOutput
{
	float4 position : SV_Position;
	float4 ray : TEXCOORD;
};

struct PushConsts
{
	float4x4 invModelViewProjection;
};

PUSH_CONSTS(PushConsts, g_PushConsts);


VSOutput main(uint vertexID : SV_VertexID) 
{
	VSOutput output;
	
	float y = -1.0 + float((vertexID & 1) << 2);
	float x = -1.0 + float((vertexID & 2) << 1);
	// set depth to 0 because we use an inverted depth buffer
    output.position = float4(x, y, 0.0, 1.0);
	output.ray = mul(g_PushConsts.invModelViewProjection, float4(output.position.xy, 0.0,  1.0));
	
	return output;
}
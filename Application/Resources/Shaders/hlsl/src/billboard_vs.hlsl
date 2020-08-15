#include "bindingHelper.hlsli"

struct VSOutput
{
	float4 position : SV_Position;
	float2 texCoord : TEXCOORD;
	float opacity : OPACITY;
	nointerpolation uint textureIndex : TEXTURE_INDEX;
};

struct PushConsts
{
	float3 position;
	float opacity;
	uint textureIndex;
	float scale;
};

struct Constants
{
	float4x4 projectionMatrix;
};

ConstantBuffer<Constants> g_Constants : REGISTER_CBV(0, 0);

PUSH_CONSTS(PushConsts, g_PushConsts);

VSOutput main(uint vertexID : SV_VertexID) 
{
	uint positionIndex = vertexID % 6;
	
	float2 positions[6] = 
	{ 
		float2(-1.0, 1.0), 
		float2(-1.0, -1.0), 
		float2(1.0, 1.0),
		float2(1.0, 1.0),
		float2(-1.0, -1.0), 
		float2(1.0, -1.0),
	};
	
	float3 pos = float3(positions[positionIndex], 0.0) * g_PushConsts.scale;
	
	VSOutput output = (VSOutput)0;
	output.position = mul(g_Constants.projectionMatrix, float4(pos + g_PushConsts.position, 1.0));
	output.texCoord = positions[positionIndex] * float2(0.5, -0.5) + 0.5;
	output.opacity = g_PushConsts.opacity;
	output.textureIndex = g_PushConsts.textureIndex;
	
	return output;
}
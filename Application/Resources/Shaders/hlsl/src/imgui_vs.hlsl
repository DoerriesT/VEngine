#include "bindingHelper.hlsli"

struct VSInput
{
	float2 position : POSITION;
	float2 texCoord : TEXCOORD0;
	float4 color : COLOR0;
};

struct VSOutput
{
	float4 position : SV_Position;
	float4 color : COLOR0;
	float2 texCoord : TEXCOORD0;
};

struct PushConsts
{
	float2 scale;
    float2 translate;
};

PUSH_CONSTS(PushConsts, g_PushConsts);

VSOutput main(uint vertexID : SV_VertexID, VSInput input) 
{
	VSOutput output;

	output.position = float4(input.position * g_PushConsts.scale + g_PushConsts.translate, 0.0, 1.0);
	output.color = input.color;
	output.texCoord = input.texCoord;

	return output;
}
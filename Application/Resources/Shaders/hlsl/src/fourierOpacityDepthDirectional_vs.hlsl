#include "bindingHelper.hlsli"
#include "common.hlsli"


struct VSOutput
{
	float4 position : SV_Position;
};

struct PushConsts
{
	uint volumeIndex;
	uint lightIndex;
};

StructuredBuffer<float4x4> g_Matrices  : REGISTER_SRV(0, 0);
StructuredBuffer<float4> g_VolumeTransforms : REGISTER_SRV(1, 0);

PUSH_CONSTS(PushConsts, g_PushConsts);


VSOutput main(float3 position : POSITION) 
{
	VSOutput output;
	
	float3 worldSpacePos;
	worldSpacePos.x = dot(g_VolumeTransforms[g_PushConsts.volumeIndex * 3 + 0], float4(position * 1.1, 1.0));
	worldSpacePos.y = dot(g_VolumeTransforms[g_PushConsts.volumeIndex * 3 + 1], float4(position * 1.1, 1.0));
	worldSpacePos.z = dot(g_VolumeTransforms[g_PushConsts.volumeIndex * 3 + 2], float4(position * 1.1, 1.0));
	
	output.position = mul(g_Matrices[g_PushConsts.lightIndex], float4(worldSpacePos, 1.0));
	
	return output;
}
#include "bindingHelper.hlsli"
#include "fourierOpacityVolumeDirectional.hlsli"
#include "common.hlsli"


struct VSOutput
{
	float4 position : SV_Position;
};

StructuredBuffer<float4x4> g_ShadowMatrices : REGISTER_SRV(MATRIX_BUFFER_BINDING, 0);
StructuredBuffer<float4> g_VolumeTransforms : REGISTER_SRV(VOLUME_TRANSFORMS_BINDING, 0);

PUSH_CONSTS(PushConsts, g_PushConsts);


VSOutput main(float3 position : POSITION) 
{
	VSOutput output;
	
	float3 worldSpacePos;
	worldSpacePos.x = dot(g_VolumeTransforms[g_PushConsts.volumeIndex * 3 + 0], float4(position * 1.1, 1.0));
	worldSpacePos.y = dot(g_VolumeTransforms[g_PushConsts.volumeIndex * 3 + 1], float4(position * 1.1, 1.0));
	worldSpacePos.z = dot(g_VolumeTransforms[g_PushConsts.volumeIndex * 3 + 2], float4(position * 1.1, 1.0));
	
	output.position = mul(g_ShadowMatrices[g_PushConsts.shadowMatrixIndex], float4(worldSpacePos, 1.0));
	
	return output;
}
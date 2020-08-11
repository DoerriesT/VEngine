#include "bindingHelper.hlsli"
#include "fourierOpacityVolume.hlsli"
#include "common.hlsli"
#include "commonEncoding.hlsli"
#include "commonFourierOpacity.hlsli"
#include "commonVolumetricFog.hlsli"


struct VSOutput
{
	float4 position : SV_Position;
	float4 worldSpacePos : WORLD_SPACE_POS;
};

StructuredBuffer<LightInfo> g_LightInfo : REGISTER_SRV(LIGHT_INFO_BINDING, 0);
StructuredBuffer<float4> g_VolumeTransforms : REGISTER_SRV(VOLUME_TRANSFORMS_BINDING, 0);

PUSH_CONSTS(PushConsts2, g_PushConsts);


VSOutput main(float3 position : POSITION) 
{
	VSOutput output;
	
	output.worldSpacePos.x = dot(g_VolumeTransforms[g_PushConsts.volumeIndex * 3 + 0], float4(position * 1.1, 1.0));
	output.worldSpacePos.y = dot(g_VolumeTransforms[g_PushConsts.volumeIndex * 3 + 1], float4(position * 1.1, 1.0));
	output.worldSpacePos.z = dot(g_VolumeTransforms[g_PushConsts.volumeIndex * 3 + 2], float4(position * 1.1, 1.0));
	output.worldSpacePos.w = 1.0;
	
	const LightInfo lightInfo = g_LightInfo[g_PushConsts.lightIndex];
	
	output.position = mul(lightInfo.viewProjection, output.worldSpacePos);
	
	return output;
}
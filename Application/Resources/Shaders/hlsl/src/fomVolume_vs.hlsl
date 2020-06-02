#include "bindingHelper.hlsli"
#include "fomVolume.hlsli"

struct VSOutput
{
	float4 position : SV_Position;
	float3 ray : RAY;
};

StructuredBuffer<LightInfo> g_LightInfo : REGISTER_SRV(LIGHT_INFO_BINDING, 0);
StructuredBuffer<VolumeInfo> g_VolumeInfo : REGISTER_SRV(VOLUME_INFO_BINDING, 0);

PUSH_CONSTS(PushConsts, g_PushConsts);

VSOutput main(float3 position : POSITION) 
{
	LightInfo lightInfo = g_LightInfo[g_PushConsts.lightIndex];
	VolumeInfo volumeInfo = g_VolumeInfo[g_PushConsts.volumeIndex];
	
	VSOutput output = (VSOutput)0;
	
	float3 worldPos;
	worldPos.x = dot(volumeInfo.localToWorld0, float4(position, 1.0));
	worldPos.y = dot(volumeInfo.localToWorld1, float4(position, 1.0));
	worldPos.z = dot(volumeInfo.localToWorld2, float4(position, 1.0));
	
	output.position = mul(lightInfo.viewProjection, float4(worldPos, 1.0));
	output.ray = worldPos - lightInfo.position;
	
	return output;
}
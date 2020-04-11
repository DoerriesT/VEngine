#include "bindingHelper.hlsli"
#include "rasterTiling.hlsli"

PUSH_CONSTS(PushConsts, g_PushConsts);

float4 main(float3 position : SV_Position) : SV_Position
{
	return mul(g_PushConsts.transform, float4(position, 1.0));
}
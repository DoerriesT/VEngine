#include "bindingHelper.hlsli"

struct PushConsts
{
	float2 srcTexelSize;
	uint width;
	uint height;
};

RWTexture2D<float> g_ResultImage : REGISTER_UAV(0, 0);
Texture2D<float> g_DepthImage : REGISTER_SRV(1, 0);
SamplerState g_PointSampler : REGISTER_SAMPLER(2, 0);

PUSH_CONSTS(PushConsts, g_PushConsts);

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	if (threadID.x >= g_PushConsts.width || threadID.y >= g_PushConsts.width)
	{
		return;
	}
	
	float4 depths = g_DepthImage.GatherRed(g_PointSampler, float2(threadID.xy * 2.0 + 0.5) * g_PushConsts.srcTexelSize);
	
	// apply min/max filter in checkerboard
	float result = (((threadID.x + threadID.y) & 1) == 0) ? min(min(depths.x, depths.y), min(depths.z, depths.w)) : max(max(depths.x, depths.y), max(depths.z, depths.w));
	
	g_ResultImage[threadID.xy] = result;
}
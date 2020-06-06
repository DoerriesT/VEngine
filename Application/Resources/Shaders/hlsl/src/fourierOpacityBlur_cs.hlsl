#include "bindingHelper.hlsli"

struct PushConsts
{
	float2 offset;
	uint resolution;
	float texelSize;
	uint horizontal;
};

RWTexture2D<float4> g_Result0Image : REGISTER_UAV(0, 0);
RWTexture2D<float4> g_Result1Image : REGISTER_UAV(1, 0);
Texture2D<float4> g_Input0Image : REGISTER_SRV(2, 0);
Texture2D<float4> g_Input1Image : REGISTER_SRV(3, 0);
SamplerState g_LinearSampler : REGISTER_SAMPLER(4, 0);

PUSH_CONSTS(PushConsts, g_PushConsts);

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	if (threadID.x >= g_PushConsts.resolution || threadID.y >= g_PushConsts.resolution)
	{
		return;
	}
	
	float4 result0 = 0.0;
	float4 result1 = 0.0;
	float2 texCoord = (threadID.xy + 0.5) * g_PushConsts.texelSize;
	
	const bool horizontal = g_PushConsts.horizontal != 0;
	
	float offsets[3] = {-1.5, 0.0, 1.5};
	float weights[3] = { 2.0 / 5.0, 1.0 / 5.0, 2.0 / 5.0};
	
	for (int i = 0; i < 3; ++i)
	{
		float2 offset = float2(offsets[i], 0.0) * g_PushConsts.texelSize;
		offset = horizontal ? offset.xy : offset.yx;
		float2 coord = texCoord + offset;
		coord = clamp(coord, g_PushConsts.texelSize, (g_PushConsts.resolution - 1) * g_PushConsts.texelSize);
		coord += g_PushConsts.offset;
		result0 += g_Input0Image.SampleLevel(g_LinearSampler, coord, 0.0) * weights[i];
		result1 += g_Input1Image.SampleLevel(g_LinearSampler, coord, 0.0) * weights[i];
	}
	
	g_Result0Image[threadID.xy] = result0;
	g_Result1Image[threadID.xy] = result1;
}
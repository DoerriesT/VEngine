#include "bindingHelper.hlsli"
#include "probeDownsample.hlsli"
#include "common.hlsli"

RWTexture2DArray<float4> g_ResultImage : REGISTER_UAV(0, 0);
Texture2DArray<float4> g_InputImage : REGISTER_SRV(1, 0);

SamplerState g_Samplers[SAMPLER_COUNT] : REGISTER_SAMPLER(0, 1);

PUSH_CONSTS(PushConsts, g_PushConsts);


[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	if (threadID.x >= g_PushConsts.resultRes || threadID.y >= g_PushConsts.resultRes)
	{
		return;
	}
	
	float2 texCoord = (threadID.xy + 0.5) * g_PushConsts.texelSize;
	g_ResultImage[threadID] = g_InputImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], float3(texCoord, threadID.z), 0.0);
}

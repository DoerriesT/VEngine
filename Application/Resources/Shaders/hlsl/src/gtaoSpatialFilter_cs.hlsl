#include "bindingHelper.hlsli"
#include "gtaoSpatialFilter.hlsli"
#include "common.hlsli"

RWTexture2D<float2> g_ResultImage : REGISTER_UAV(RESULT_IMAGE_BINDING, 0);
Texture2D<float2> g_InputImage : REGISTER_SRV(INPUT_IMAGE_BINDING, 0);

SamplerState g_Samplers[SAMPLER_COUNT] : REGISTER_SAMPLER(0, 1);

PUSH_CONSTS(PushConsts, g_PushConsts);


[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	if (threadID.x >= g_PushConsts.width || threadID.y >= g_PushConsts.height)
	{
		return;
	}

	float2 texCoord = float2(threadID.xy + 0.5) * g_PushConsts.texelSize;
	float2 center = g_InputImage.Load(int3(threadID.xy, 0)).xy;
	float rampMaxInv = 1.0 / (center.y * 0.1);
	
	float totalAo = 0.0;
	float totalWeight = 0.0;
	
	int offset = 0;//uFrame % 2;
	for(int y = -2 + offset; y < 2 + offset; ++y)
	{
		for (int x = -1 - offset; x < 3 - offset; ++x)
		{
			float2 tap = g_InputImage.SampleLevel(g_Samplers[SAMPLER_POINT_CLAMP], g_PushConsts.texelSize * float2(x, y) + texCoord, 0.0).xy;
			float weight = saturate(1.0 - (abs(tap.y - center.y) * rampMaxInv));
			totalAo += tap.x * weight;
			totalWeight += weight;
		}
	}
	
	float ao = totalAo / totalWeight;
	
	g_ResultImage[threadID.xy] = float2(ao, center.y);
}
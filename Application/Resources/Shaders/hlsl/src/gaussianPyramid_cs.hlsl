#include "bindingHelper.hlsli"
#include "gaussianPyramid.hlsli"
#include "common.hlsli"

RWTexture2D<float4> g_ResultImage : REGISTER_UAV(RESULT_IMAGE_BINDING, 0);
Texture2D<float4> g_InputImage : REGISTER_SRV(INPUT_IMAGE_BINDING, 0);

SamplerState g_Samplers[SAMPLER_COUNT] : REGISTER_SAMPLER(0, 1);

PUSH_CONSTS(PushConsts, g_PushConsts);

static const float offsets[] = { -3.71428561, -1.85714281, 0.0, 1.85714281, 3.71428561 };
static const float weights[] = { 0.0445859879, 0.245222926, 0.420382172, 0.245222926, 0.0445859879 };

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	if (threadID.x >= g_PushConsts.width || threadID.y >= g_PushConsts.height)
	{
		return;
	}
	
	float3 result = 0.0;
	
	if (g_PushConsts.horizontal != 0)
	{
		float2 texCoord = (threadID.xy * float2(2.0, 1.0) + 0.5) * g_PushConsts.srcTexelSize;
	
		for (int i = 0; i < 5; ++i)
		{
			result += g_InputImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], texCoord + float2(offsets[i], 0.0) * g_PushConsts.srcTexelSize, 0.0).rgb * weights[i];
		}
	}
	else
	{
		float2 texCoord = (threadID.xy * float2(1.0, 2.0) + 0.5) * g_PushConsts.srcTexelSize;
	
		for (int i = 0; i < 5; ++i)
		{
			result += g_InputImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], texCoord + float2(0.0, offsets[i]) * g_PushConsts.srcTexelSize, 0.0).rgb * weights[i];
		}
	}
	
	g_ResultImage[threadID.xy] = float4(result, 1.0);
}
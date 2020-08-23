#include "bindingHelper.hlsli"
#include "bloomDownsample.hlsli"
#include "common.hlsli"

RWTexture2D<float4> g_ResultImage : REGISTER_UAV(RESULT_IMAGE_BINDING, 0);
Texture2D<float4> g_InputImage : REGISTER_SRV(INPUT_IMAGE_BINDING, 0);

SamplerState g_Samplers[SAMPLER_COUNT] : REGISTER_SAMPLER(0, 1);

PUSH_CONSTS(PushConsts, g_PushConsts);

float4 addTap(float3 tap, float weight, bool lumaWeighted)
{
	weight *= lumaWeighted ? 1.0 / (1.0 + dot(tap, float3(0.2126, 0.7152, 0.0722))) : 1.0;
	return float4(weight * tap, weight);
}

[numthreads(16, 16, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	if (threadID.x >= g_PushConsts.width || threadID.y >= g_PushConsts.height)
	{
		return;
	}
	
	const float2 texelSize = g_PushConsts.texelSize;
	const float2 texCoord = float2(threadID.xy + 0.5) * texelSize;
	
	float4 sum = 0.0;
	
	const bool doWeightedAverage = (g_PushConsts.doWeightedAverage != 0);
	
	sum += addTap(g_InputImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], texCoord + float2(-texelSize.x, -texelSize.y),       0.0).rgb, 0.125, doWeightedAverage);
	sum += addTap(g_InputImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], texCoord + float2( 0.0,         -texelSize.y),       0.0).rgb, 0.25,  doWeightedAverage);
	sum += addTap(g_InputImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], texCoord + float2( texelSize.x, -texelSize.y),       0.0).rgb, 0.125, doWeightedAverage);
	
	sum += addTap(g_InputImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], texCoord + float2(-texelSize.x, -texelSize.y) * 0.5, 0.0).rgb, 0.5,   doWeightedAverage);
	sum += addTap(g_InputImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], texCoord + float2( texelSize.x, -texelSize.y) * 0.5, 0.0).rgb, 0.5,   doWeightedAverage);
	
	sum += addTap(g_InputImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], texCoord + float2(-texelSize.x,          0.0),       0.0).rgb, 0.25,  doWeightedAverage);
	sum += addTap(g_InputImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], texCoord,                                            0.0).rgb, 0.5,   doWeightedAverage);
	sum += addTap(g_InputImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], texCoord + float2( texelSize.x,          0.0),       0.0).rgb, 0.25,  doWeightedAverage);
	
	sum += addTap(g_InputImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], texCoord + float2(-texelSize.x,  texelSize.y) * 0.5, 0.0).rgb, 0.5,   doWeightedAverage);
	sum += addTap(g_InputImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], texCoord + float2( texelSize.x,  texelSize.y) * 0.5, 0.0).rgb, 0.5,   doWeightedAverage);
	
	sum += addTap(g_InputImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], texCoord + float2(-texelSize.x,  texelSize.y),       0.0).rgb, 0.125, doWeightedAverage);
	sum += addTap(g_InputImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], texCoord + float2( 0.0,          texelSize.y),       0.0).rgb, 0.25,  doWeightedAverage);
	sum += addTap(g_InputImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], texCoord + float2( texelSize.x,  texelSize.y),       0.0).rgb, 0.125, doWeightedAverage);
	
	sum.rgb /= sum.a;
	
	g_ResultImage[threadID.xy] = float4(sum.rgb, 1.0);
}
#include "bindingHelper.hlsli"
#include "bloomUpsample.hlsli"
#include "common.hlsli"

RWTexture2D<float4> g_ResultImage : REGISTER_UAV(RESULT_IMAGE_BINDING, 0);
Texture2D<float4> g_InputImage : REGISTER_SRV(INPUT_IMAGE_BINDING, 0);
Texture2D<float4> g_PrevResultImage : REGISTER_SRV(PREV_RESULT_IMAGE_BINDING, 0);

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
	
	const float radius = 1.7;
	
	float3 sum = 0.0;
	
	sum += 1.0 * g_InputImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], texCoord + float2(-texelSize.x, -texelSize.y) * radius, 0.0).rgb;
	sum += 2.0 * g_InputImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], texCoord + float2( 0.0,         -texelSize.y) * radius, 0.0).rgb;
	sum += 1.0 * g_InputImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], texCoord + float2( texelSize.x, -texelSize.y) * radius, 0.0).rgb;
	
	sum += 2.0 * g_InputImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], texCoord + float2(-texelSize.x,          0.0) * radius, 0.0).rgb;
	sum += 4.0 * g_InputImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], texCoord,                                               0.0).rgb;
	sum += 2.0 * g_InputImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], texCoord + float2( texelSize.x,          0.0) * radius, 0.0).rgb;
	
	sum += 1.0 * g_InputImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], texCoord + float2(-texelSize.x,  texelSize.y) * radius, 0.0).rgb;
	sum += 2.0 * g_InputImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], texCoord + float2( 0.0,          texelSize.y) * radius, 0.0).rgb;
	sum += 1.0 * g_InputImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], texCoord + float2( texelSize.x,  texelSize.y) * radius, 0.0).rgb;
	
	sum *= (1.0 / 16.0);
	
	if (g_PushConsts.addPrevious != 0)
	{
		sum += g_PrevResultImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], texCoord, 0.0).rgb;
		sum *= 0.5;
	}
	
	g_ResultImage[threadID.xy] = float4(sum, 1.0);
}
#include "bindingHelper.hlsli"
#include "gtaoTemporalFilter.hlsli"

RWTexture2D<float2> g_ResultImage : REGISTER_UAV(RESULT_IMAGE_BINDING, RESULT_IMAGE_SET);
Texture2D<float2> g_InputImage : REGISTER_SRV(INPUT_IMAGE_BINDING, INPUT_IMAGE_SET);
Texture2D<float2> g_HistoryImage : REGISTER_SRV(HISTORY_IMAGE_BINDING, HISTORY_IMAGE_SET);
Texture2D<float2> g_VelocityImage : REGISTER_SRV(VELOCITY_IMAGE_BINDING, VELOCITY_IMAGE_SET);
SamplerState g_LinearSampler : REGISTER_SAMPLER(LINEAR_SAMPLER_BINDING, LINEAR_SAMPLER_SET);
ConstantBuffer<Constants> g_Constants : REGISTER_CBV(CONSTANT_BUFFER_BINDING, CONSTANT_BUFFER_SET);

//PUSH_CONSTS(PushConsts, g_PushConsts);

float calulateAlpha(float frameTime, float convergenceTime)
{
	return exp(-frameTime / convergenceTime);
}

float length2(float3 v)
{
	return dot(v, v);
}

float projectDepth(float n, float f, float z)
{
	return (((-(f + n) / (f - n)) * z + (-2.0 * f * n) / (f - n)) / -z) * 0.5 + 0.5;
}

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	if (threadID.x >= g_Constants.width || threadID.y >= g_Constants.height)
	{
		return;
	}
	
	float2 center = g_InputImage.Load(int3(threadID.xy, 0)).xy;
	
	if (g_Constants.ignoreHistory != 0)
	{
		g_ResultImage[threadID.xy] = center;
		return;
	}
	
	float ao = center.x;
	float depth = center.y;
	
	// get previous frames ao value
	float2 velocity = g_VelocityImage.Load(int3(threadID.xy, 0)).xy;
	float2 texCoord = float2(threadID.xy + 0.5) * g_Constants.texelSize;
	float2 reprojectedCoord = texCoord - velocity;
	float2 previousAo = g_HistoryImage.SampleLevel(g_LinearSampler, reprojectedCoord, 0.0).xy;
    
	// is the reprojected coordinate inside the frame?
	float insideFrame = all(abs(reprojectedCoord - 0.5) < 0.5) ? 1.0 : 0.0;
	
	float4 previousWorldPos = mul(g_Constants.prevInvViewProjection, float4(reprojectedCoord.xy * float2(2.0, -2.0) - float2(1.0, -1.0), projectDepth(g_Constants.nearPlane, g_Constants.farPlane, -previousAo.y), 1.0));
	previousWorldPos.xyz /= previousWorldPos.w;
	
	float4 currentWorldPos = mul(g_Constants.invViewProjection, float4(texCoord * float2(2.0, -2.0) - float2(1.0, -1.0), projectDepth(g_Constants.nearPlane, g_Constants.farPlane, -depth), 1.0));
	currentWorldPos.xyz /= currentWorldPos.w;
    
	float disocclusionWeight = 1.0 - saturate(length2(abs(currentWorldPos.xyz - previousWorldPos.xyz)) * (1.0 / (0.5 * 0.5)));
	
	// based on depth, how likely describes the value the same point?
	float depthWeight = saturate(1.0 - ((depth - previousAo.y) / (depth * 0.01)));
	
	// neighborhood clamping
	//vec2 minMax = vec2(1e6, -1e6);
	//
	//// determine neighborhood
	//{
	//	// top left
	//	{
	//		float S = textureLod(sampler2D(uInputImage, uLinearSampler), 1.5 * vec2(-texelSize.x, texelSize.y) + texCoord, 0).x;
	//		minMax.x = min(S, minMax.x);
	//		minMax.y = max(S, minMax.y);
	//	}
	//	// top right
	//	{
	//		float S = textureLod(sampler2D(uInputImage, uLinearSampler), 1.5 * vec2(texelSize.x, texelSize.y) + texCoord, 0).x;
	//		minMax.x = min(S, minMax.x);
	//		minMax.y = max(S, minMax.y);
	//	}
	//	// bottom left
	//	{
	//		float S = textureLod(sampler2D(uInputImage, uLinearSampler), 1.5 * vec2(-texelSize.x, -texelSize.y) + texCoord, 0).x;
	//		minMax.x = min(S, minMax.x);
	//		minMax.y = max(S, minMax.y);
	//	}
	//	// bottom right
	//	{
	//		float S = textureLod(sampler2D(uInputImage, uLinearSampler), 1.5 * vec2(texelSize.x, -texelSize.y) + texCoord, 0).x;
	//		minMax.x = min(S, minMax.x);
	//		minMax.y = max(S, minMax.y);
	//	}
	//	// center
	//	minMax.x = min(ao, minMax.x);
	//	minMax.y = max(ao, minMax.y);
	//}
	//
	//previousAo.x = clamp(previousAo.x, minMax.x, minMax.y);
	
	//float convergenceAlpha = calulateAlpha(uFrameTime, velocityWeight * 0.5 * (1.0 / 60.0) * 24.0);
	
	ao = lerp(ao, previousAo.x, (23.0 / 24.0) * insideFrame * disocclusionWeight);
	
	g_ResultImage[threadID.xy] = float2(ao, depth);
}
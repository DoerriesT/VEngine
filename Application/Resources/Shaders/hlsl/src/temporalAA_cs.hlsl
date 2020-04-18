#include "bindingHelper.hlsli"
#include "temporalAA.hlsli"

RWTexture2D<float4> g_ResultImage : REGISTER_UAV(RESULT_IMAGE_BINDING, RESULT_IMAGE_SET);
Texture2D<float4> g_InputImage : REGISTER_SRV(INPUT_IMAGE_BINDING, INPUT_IMAGE_SET);
Texture2D<float4> g_HistoryImage : REGISTER_SRV(HISTORY_IMAGE_BINDING, HISTORY_IMAGE_SET);
Texture2D<float> g_DepthImage : REGISTER_SRV(DEPTH_IMAGE_BINDING, DEPTH_IMAGE_SET);
Texture2D<float2> g_VelocityImage : REGISTER_SRV(VELOCITY_IMAGE_BINDING, VELOCITY_IMAGE_SET);
SamplerState g_LinearSampler : REGISTER_SAMPLER(LINEAR_SAMPLER_BINDING, LINEAR_SAMPLER_SET);
ByteAddressBuffer g_ExposureData : REGISTER_SRV(EXPOSURE_DATA_BUFFER_BINDING, EXPOSURE_DATA_BUFFER_SET);


PUSH_CONSTS(PushConsts, g_PushConsts);

float3 rgbToYcocg(float3 color)
{
	float3 result;
	result.x = dot(color, float3(  1, 2,  1 ) );
	result.y = dot(color, float3(  2, 0, -2 ) );
	result.z = dot(color, float3( -1, 2, -1 ) );
	
	return result;
}

float3 ycocgToRgb(float3 color )
{
	float y  = color.x * 0.25;
	float co = color.y * 0.25;
	float cg = color.z * 0.25;

	float3 result;
	result.r = y + co - cg;
	result.g = y + cg;
	result.b = y - co - cg;

	return result;
}

float3 sampleHistory(float2 texCoord, float4 rtMetrics)
{
	const float sharpening = g_PushConsts.bicubicSharpness;  // [0.0, 1.0]

	float2 samplePos = texCoord * rtMetrics.xy;
	float2 tc1 = floor(samplePos - 0.5) + 0.5;
	float2 f = samplePos - tc1;
	float2 f2 = f * f;
	float2 f3 = f * f2;

	// Catmull-Rom weights
	const float c = sharpening;
	float2 w0 = -(c)       * f3 + (2.0 * c)        * f2 - (c * f);
	float2 w1 =  (2.0 - c) * f3 - (3.0 - c)        * f2            + 1.0;
	float2 w2 = -(2.0 - c) * f3 + (3.0 -  2.0 * c) * f2 + (c * f);
	float2 w3 =  (c)       * f3 - (c)              * f2;

	float2 w12  = w1 + w2;
	float2 tc0  = (tc1 - 1.0)      * rtMetrics.zw;
	float2 tc3  = (tc1 + 2.0)      * rtMetrics.zw;
	float2 tc12 = (tc1 + w2 / w12) * rtMetrics.zw;
	
	// Bicubic filter using bilinear lookups, skipping the 4 corner texels
	float4 filtered = float4(g_HistoryImage.SampleLevel(g_LinearSampler, float2(tc12.x, tc0.y ), 0.0).rgb, 1.0) * (w12.x *  w0.y) +
	                  float4(g_HistoryImage.SampleLevel(g_LinearSampler, float2(tc0.x,  tc12.y), 0.0).rgb, 1.0) * ( w0.x * w12.y) +
	                  float4(g_HistoryImage.SampleLevel(g_LinearSampler, float2(tc12.x, tc12.y), 0.0).rgb, 1.0) * (w12.x * w12.y) +  // Center pixel
	                  float4(g_HistoryImage.SampleLevel(g_LinearSampler, float2(tc3.x,  tc12.y), 0.0).rgb, 1.0) * ( w3.x * w12.y) +
	                  float4(g_HistoryImage.SampleLevel(g_LinearSampler, float2(tc12.x, tc3.y ), 0.0).rgb, 1.0) * (w12.x *  w3.y);
	
	return filtered.rgb * (1.0 / filtered.a);
}

float3 clipAABB(float3 p, float3 aabbMin, float3 aabbMax)
{
    //Clips towards AABB center for better perfomance
    float3 center   = 0.5 * (aabbMax + aabbMin);
    float3 halfSize = 0.5 * (aabbMax - aabbMin) + 1e-5;
    //Relative position from the center
    float3 clip     = p - center;
    //Normalize relative position
    float3 unit     = clip / halfSize;
    float3 absUnit  = abs(unit);
    float maxUnit = max(absUnit.x, max(absUnit.y, absUnit.z));
	
	return (maxUnit > 1.0) ? clip * (1.0 / maxUnit) + center : p;
}

float2 weightedLerpFactors(float weightA, float weightB, float blend)
{
	float blendA = (1.0 - blend) * weightA;
	float blendB = blend * weightB;
	float invBlend = 1.0 / (blendA + blendB);
	blendA *= invBlend;
	blendB *= invBlend;
	return float2(blendA, blendB);
}

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	if (threadID.x >= g_PushConsts.width || threadID.y >= g_PushConsts.height)
	{
		return;
	}
	
	if (g_PushConsts.ignoreHistory != 0)
	{
		g_ResultImage[threadID.xy] = float4(g_InputImage.Load(int3(threadID.xy, 0)).rgb, 1.0);
		return;
	}
	
	// find frontmost velocity in neighborhood
	int2 velocityCoordOffset = 0;
	{
		float closestDepth = 0.0;
		for (int y = -1; y < 2; ++y)
		{
			for (int x = -1; x < 2; ++x)
			{
				float depth = g_DepthImage.Load(int3(threadID.xy + int2(x,y), 0)).x;
				velocityCoordOffset = depth > closestDepth ? int2(x, y) : velocityCoordOffset;
				closestDepth = depth > closestDepth ? depth : closestDepth;
			}
		}
	}
	
	
	float2 velocity = g_VelocityImage.Load(int3(threadID.xy + velocityCoordOffset, 0)).xy;
	
	float3 currentColor = rgbToYcocg(g_InputImage.Load(int3(threadID.xy, 0)).rgb);
	float3 neighborhoodMin = currentColor;
	float3 neighborhoodMax = currentColor;
	float3 neighborhoodMinPlus = currentColor;
	float3 neighborhoodMaxPlus = currentColor;
	
	// sample neighborhood
	{
		for (int y = -1; y < 2; ++y)
		{
			for (int x = -1; x < 2; ++x)
			{
				float3 tap = rgbToYcocg(g_InputImage.Load(int3(threadID.xy + int2(x,y), 0)).rgb);
				neighborhoodMin = min(tap, neighborhoodMin);
				neighborhoodMax = max(tap, neighborhoodMax);
				neighborhoodMinPlus = (x == 0 || y == 0) ? min(tap, neighborhoodMinPlus) : neighborhoodMinPlus;
				neighborhoodMaxPlus = (x == 0 || y == 0) ? max(tap, neighborhoodMaxPlus) : neighborhoodMaxPlus;
			}
		}
	}
	
	neighborhoodMin = lerp(neighborhoodMin, neighborhoodMinPlus, 0.5);
	neighborhoodMax = lerp(neighborhoodMax, neighborhoodMaxPlus, 0.5);
	
	uint2 imageDims;
	g_InputImage.GetDimensions(imageDims.x, imageDims.y);
	float2 texSize = (float2)imageDims;
	float2 texelSize = 1.0 / texSize;
	float2 texCoord = (float2(threadID.xy) + 0.5) * texelSize;
	float2 prevTexCoord = texCoord - velocity;
	
	// history is pre-exposed -> convert from previous frame exposure to current frame exposure
	float exposureConversionFactor = asfloat(g_ExposureData.Load(1 << 2)); // 0 = current frame exposure | 1 = previous frame to current frame exposure
	
	float3 historyColor = rgbToYcocg(max(sampleHistory(prevTexCoord, float4(texSize, texelSize)).rgb, 0.0) * exposureConversionFactor);
	historyColor = clipAABB(historyColor, neighborhoodMin, neighborhoodMax);
	
	float subpixelCorrection = abs(frac(max(abs(prevTexCoord.x) * texSize.x, abs(prevTexCoord.y) * texSize.y)) * 2.0 - 1.0);
	
	float alpha = lerp(0.05, 0.8, subpixelCorrection);
	alpha *= g_PushConsts.jitterOffsetWeight;
	alpha = prevTexCoord.x <= 0.0 || prevTexCoord.x >= 1.0 || prevTexCoord.y <= 0.0 || prevTexCoord.y >= 1.0 ? 1.0 : alpha;
	
	float2 factors = weightedLerpFactors(1.0 / (historyColor.x + 4.0), 1.0 / (currentColor.x + 4.0), alpha);
	
	float3 result = historyColor * factors.x + currentColor * factors.y;
	
	g_ResultImage[threadID.xy] = float4(ycocgToRgb(result), 1.0);
}
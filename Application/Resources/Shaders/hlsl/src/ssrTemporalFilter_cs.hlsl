#include "bindingHelper.hlsli"
#include "ssrTemporalFilter.hlsli"

RWTexture2D<float4> g_ResultImage : REGISTER_UAV(RESULT_IMAGE_BINDING, RESULT_IMAGE_SET);
Texture2D<float4> g_ColorRayDepthImage : REGISTER_SRV(COLOR_RAY_DEPTH_IMAGE_BINDING, COLOR_RAY_DEPTH_IMAGE_SET);
Texture2D<float> g_MaskImage : REGISTER_SRV(MASK_IMAGE_BINDING, MASK_IMAGE_SET);
Texture2D<float4> g_HistoryImage : REGISTER_SRV(HISTORY_IMAGE_BINDING, HISTORY_IMAGE_SET);
SamplerState g_LinearSampler : REGISTER_SAMPLER(LINEAR_SAMPLER_BINDING, LINEAR_SAMPLER_SET);
ConstantBuffer<Constants> g_Constants : REGISTER_CBV(CONSTANT_BUFFER_BINDING, CONSTANT_BUFFER_SET);
ByteAddressBuffer g_ExposureData : REGISTER_SRV(EXPOSURE_DATA_BUFFER_BINDING, EXPOSURE_DATA_BUFFER_SET);

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

void resolverAABB(int2 coord, inout float4 minColor, inout float4 maxColor, inout float4 filteredColor)
{
	float4 m1 = 0.0;
	float4 m2 = 0.0;
	for (int y = 0; y < 3; ++y)
	{
		for (int x = 0; x < 3; ++x)
		{
			float4 tap;
			tap.rgb = g_ColorRayDepthImage.Load(int3(coord + int2(x, y) - 1, 0)).rgb;
			tap.a = g_MaskImage.Load(int3(coord + int2(x, y) - 1, 0)).x;
			
			m1 += tap;
			m2 += tap * tap;
			filteredColor = x == 1 && y == 1 ? tap : filteredColor;
		}
	}	
	
	float4 mean = m1 * (1.0 / 9.0);
	float4 stddev = sqrt(max((m2  * (1.0 / 9.0) - mean * mean), 1e-7));
	
	float wideningFactor = 4.25;
	
	minColor = -stddev * wideningFactor + mean;
	maxColor = stddev * wideningFactor + mean;
}

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	if (threadID.x >= g_Constants.width || threadID.y >= g_Constants.height)
	{
		return;
	}

	float2 texCoord = float2(threadID.xy + 0.5) * float2(g_Constants.texelWidth, g_Constants.texelHeight);
	float4 colorRayDepth = g_ColorRayDepthImage.Load(int3(threadID.xy, 0));
	float rayDepth = colorRayDepth.w;
	float4 reprojectedCoord = mul(g_Constants.reprojectionMatrix, float4(texCoord * 2.0 - 1.0, rayDepth, 1.0));
	reprojectedCoord.xy /= reprojectedCoord.w;
	reprojectedCoord.xy = reprojectedCoord.xy * 0.5 + 0.5;
	
	float4 neighborhoodMin = 0.0;
	float4 neighborhoodMax = 0.0;
	float4 current = 0.0;
	resolverAABB(threadID.xy, neighborhoodMin, neighborhoodMax, current);
	
	float2 prevTexCoord = current.a == 0.0 ? texCoord : reprojectedCoord.xy;
	
	float4 history = current;
	
	if (g_Constants.ignoreHistory == 0)
	{
		// history is pre-exposed -> convert from previous frame exposure to current frame exposure
		float exposureConversionFactor = asfloat(g_ExposureData.Load(1 << 2)); // 0 = current frame exposure | 1 = previous frame to current frame exposure
	
		history = g_HistoryImage.SampleLevel(g_LinearSampler, prevTexCoord, 0.0);
		history.rgb *= exposureConversionFactor;
		history.rgb = clipAABB(history.rgb, neighborhoodMin.rgb, neighborhoodMax.rgb);
		history.a = clamp(history.a, neighborhoodMin.a, neighborhoodMax.a);
	}
	
	float4 result = lerp(history, current, 0.02);
	result = isnan(result) || isinf(result) ? 0.0 : result;
	
	//vec4 historyColorMask = textureLod(uHistoryImage, prevTexCoord, 0.0); 
	//historyColorMask.rgb = rgbToYcocg(historyColorMask.rgb);
	//historyColorMask.rgb = clipAABB(historyColorMask.rgb, neighborhoodMin, neighborhoodMax);
	//
	//vec2 domainSize = vec2(textureSize(uColorRayDepthImage, 0).xy);
	//float subpixelCorrection = abs(fract(max(abs(prevTexCoord.x) * domainSize.x, abs(prevTexCoord.y) * domainSize.y)) * 2.0 - 1.0);
	//
	//float alpha = mix(0.005, 0.3, subpixelCorrection);
	//alpha = prevTexCoord.x <= 0.0 || prevTexCoord.x >= 1.0 || prevTexCoord.y <= 0.0 || prevTexCoord.y >= 1.0 ? 1.0 : alpha;
	//
	//vec2 factors = weightedLerpFactors(1.0 / (dot(historyColorMask.rgb, LUMA_RGB_TUPLE) + 4.0), 1.0 / (dot(currentColor.rgb, LUMA_RGB_TUPLE) + 4.0), alpha);
	//
	//vec4 result = historyColorMask * factors.x + vec4(currentColor, texelFetch(uMaskImage, ivec2(gl_GlobalInvocationID.xy), 0).x) * factors.y;
	//result.rgb = ycocgToRgb(result.rgb);
	
	g_ResultImage[threadID.xy] = result;
}
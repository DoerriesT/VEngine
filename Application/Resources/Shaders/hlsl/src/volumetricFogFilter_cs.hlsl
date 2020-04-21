#include "bindingHelper.hlsli"
#include "volumetricFogFilter.hlsli"
#include "commonFilter.hlsli"


#define VOLUME_DEPTH (64)
#define VOLUME_NEAR (0.5)
#define VOLUME_FAR (64.0)

RWTexture3D<float4> g_ResultImage : REGISTER_UAV(RESULT_IMAGE_BINDING, RESULT_IMAGE_SET);
Texture3D<float4> g_InputImage : REGISTER_SRV(INPUT_IMAGE_BINDING, INPUT_IMAGE_SET);
Texture3D<float4> g_HistoryImage : REGISTER_SRV(HISTORY_IMAGE_BINDING, HISTORY_IMAGE_SET);
SamplerState g_LinearSampler : REGISTER_SAMPLER(LINEAR_SAMPLER_BINDING, LINEAR_SAMPLER_SET);
ConstantBuffer<Constants> g_Constants : REGISTER_CBV(CONSTANT_BUFFER_BINDING, CONSTANT_BUFFER_SET);
ByteAddressBuffer g_ExposureData : REGISTER_SRV(EXPOSURE_DATA_BUFFER_BINDING, EXPOSURE_DATA_BUFFER_SET);

PUSH_CONSTS(PushConsts, g_PushConsts);

float getViewSpaceDistance(float texelDepth)
{
	float d = texelDepth * (1.0 / VOLUME_DEPTH);
	return VOLUME_NEAR * exp2(d * (log2(VOLUME_FAR / VOLUME_NEAR)));
}

float3 calcWorldSpacePos(float3 texelCoord)
{
	uint3 imageDims;
	g_ResultImage.GetDimensions(imageDims.x, imageDims.y, imageDims.z);
	float2 uv = texelCoord.xy / float2(imageDims.xy);
	
	float3 pos = lerp(g_Constants.frustumCornerTL, g_Constants.frustumCornerTR, uv.x);
	pos = lerp(pos, lerp(g_Constants.frustumCornerBL, g_Constants.frustumCornerBR, uv.x), uv.y);
	
	pos *= getViewSpaceDistance(texelCoord.z) / VOLUME_FAR;
	
	pos += g_Constants.cameraPos;
	
	return pos;
}

// based on advances.realtimerendering.com/s2016/Filmic%20SMAA%20v7.pptx
float4 sampleHistory(float2 texCoord, float4 rtMetrics, float d)
{
	const float sharpening = 0.5;// [0.0, 1.0]

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
	float4 filtered = g_HistoryImage.SampleLevel(g_LinearSampler, float3(tc12.x, tc0.y , d), 0.0) * (w12.x *  w0.y) +
	                  g_HistoryImage.SampleLevel(g_LinearSampler, float3(tc0.x,  tc12.y, d), 0.0) * ( w0.x * w12.y) +
	                  g_HistoryImage.SampleLevel(g_LinearSampler, float3(tc12.x, tc12.y, d), 0.0) * (w12.x * w12.y) +  // Center pixel
	                  g_HistoryImage.SampleLevel(g_LinearSampler, float3(tc3.x,  tc12.y, d), 0.0) * ( w3.x * w12.y) +
	                  g_HistoryImage.SampleLevel(g_LinearSampler, float3(tc12.x, tc3.y , d), 0.0) * (w12.x *  w3.y);
	
	float weightSum = (w12.x *  w0.y) +
	                  ( w0.x * w12.y) +
	                  (w12.x * w12.y) +  // Center pixel
	                  ( w3.x * w12.y) +
	                  (w12.x *  w3.y);
	
	return max(filtered * (1.0 / weightSum), 0.0);
}

void resolverAABB(int3 coord, inout float4 minColor, inout float4 maxColor)
{
	float3 texSize;
	g_HistoryImage.GetDimensions(texSize.x, texSize.y, texSize.z);
	float3 texelSize = 1.0 / texSize;
	
	float4 m1 = 0.0;
	float4 m2 = 0.0;
	float4 maxValue = 0.0;
	
	{
		float4 tap = g_InputImage.SampleLevel(g_LinearSampler, texelSize * (float3(coord + 0.5) + float3(-0.75, -0.75, -0.75)), 0.0);
		m1 += tap;
		m2 += tap * tap;
		maxValue = max(maxValue, tap);
	}
	{
		float4 tap = g_InputImage.SampleLevel(g_LinearSampler, texelSize * (float3(coord + 0.5) + float3( 0.75, -0.75, -0.75)), 0.0);
		m1 += tap;
		m2 += tap * tap;
		maxValue = max(maxValue, tap);
	}
	{
		float4 tap = g_InputImage.SampleLevel(g_LinearSampler, texelSize * (float3(coord + 0.5) + float3(-0.75,  0.75, -0.75)), 0.0);
		m1 += tap;
		m2 += tap * tap;
		maxValue = max(maxValue, tap);
	}
	{
		float4 tap = g_InputImage.SampleLevel(g_LinearSampler, texelSize * (float3(coord + 0.5) + float3( 0.75,  0.75, -0.75)), 0.0);
		m1 += tap;
		m2 += tap * tap;
		maxValue = max(maxValue, tap);
	}
	{
		float4 tap = g_InputImage.SampleLevel(g_LinearSampler, texelSize * (float3(coord + 0.5) + float3(-0.75, -0.75,  0.75)), 0.0);
		m1 += tap;
		m2 += tap * tap;
		maxValue = max(maxValue, tap);
	}
	{
		float4 tap = g_InputImage.SampleLevel(g_LinearSampler, texelSize * (float3(coord + 0.5) + float3( 0.75, -0.75,  0.75)), 0.0);
		m1 += tap;
		m2 += tap * tap;
		maxValue = max(maxValue, tap);
	}
	{
		float4 tap = g_InputImage.SampleLevel(g_LinearSampler, texelSize * (float3(coord + 0.5) + float3(-0.75,  0.75,  0.75)), 0.0);
		m1 += tap;
		m2 += tap * tap;
		maxValue = max(maxValue, tap);
	}
	{
		float4 tap = g_InputImage.SampleLevel(g_LinearSampler, texelSize * (float3(coord + 0.5) + float3( 0.75,  0.75,  0.75)), 0.0);
		m1 += tap;
		m2 += tap * tap;
		maxValue = max(maxValue, tap);
	}	
	
	float4 mean = m1 * (1.0 / 8.0);
	float4 stddev = sqrt(max((m2  * (1.0 / 8.0) - mean * mean), 1e-7));
	
	float wideningFactor = 10.0;
	
	minColor = -stddev * wideningFactor + mean;
	maxColor = stddev * wideningFactor + mean;
	minColor = 0.0;
	maxColor = maxValue * 4.0;
}

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID, uint3 groupThreadID : SV_GroupThreadID, uint groupIdx : SV_GroupIndex, uint3 groupID : SV_GroupID)
{
	uint3 froxelID = threadID.xyz;
	
	float3 texSize;
	g_HistoryImage.GetDimensions(texSize.x, texSize.y, texSize.z);
	float3 texelSize = 1.0 / texSize;
	
	float4 result = g_InputImage.Load(int4(froxelID, 0));
	
	// reproject and combine with previous result from previous frame
	if (g_Constants.ignoreHistory == 0)
	{
		float4 prevViewSpacePos = mul(g_Constants.prevViewMatrix, float4(calcWorldSpacePos(froxelID + 0.5), 1.0));
		
		float z = -prevViewSpacePos.z;
		float d = (log2(max(0, z * (1.0 / VOLUME_NEAR))) * (1.0 / log2(VOLUME_FAR / VOLUME_NEAR)));

		float4 prevClipSpacePos = mul(g_Constants.prevProjMatrix, prevViewSpacePos);
		float3 prevTexCoord = float3((prevClipSpacePos.xy / prevClipSpacePos.w) * 0.5 + 0.5, d);
		prevTexCoord.xy = prevTexCoord.xy * g_Constants.reprojectedTexCoordScaleBias.xy + g_Constants.reprojectedTexCoordScaleBias.zw;
		
		bool validCoord = all(prevTexCoord >= 0.0 && prevTexCoord <= 1.0);
		float4 prevResult = 0.0;
		if (validCoord)
		{
			prevResult = g_PushConsts.advancedFilter ? 
							sampleHistory(prevTexCoord.xy, float4(texSize.xy, texelSize.xy), prevTexCoord.z) :
							g_HistoryImage.SampleLevel(g_LinearSampler, prevTexCoord, 0.0);
		}
		
		// prevResult.rgb is pre-exposed -> convert from previous frame exposure to current frame exposure
		prevResult.rgb *= asfloat(g_ExposureData.Load(1 << 2)); // 0 = current frame exposure | 1 = previous frame to current frame exposure
		
		if (g_PushConsts.neighborHoodClamping != 0)
		{
			float4 minColor, maxColor;
			resolverAABB(froxelID, minColor, maxColor);
			prevResult.rgb = clipAABB(prevResult.rgb, minColor.rgb, maxColor.rgb);
			prevResult.a = clamp(prevResult.a, minColor.a, maxColor.a);
		}
		
		//prevResult.rgb = simpleTonemap(prevResult.rgb);
		//result.rgb = simpleTonemap(result.rgb);
		
		result = lerp(prevResult, result, validCoord ? g_PushConsts.alpha : 1.0);
		
		//result.rgb = inverseSimpleTonemap(result.rgb);
	}
	
	g_ResultImage[froxelID] = result;
}
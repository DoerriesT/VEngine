#include "bindingHelper.hlsli"
#include "volumetricFogFilter2.hlsli"


#define VOLUME_DEPTH (64)
#define VOLUME_NEAR (0.5)
#define VOLUME_FAR (64.0)

RWTexture3D<float4> g_ResultImage : REGISTER_UAV(RESULT_IMAGE_BINDING, 0);
Texture3D<float4> g_InputImage : REGISTER_SRV(INPUT_IMAGE_BINDING, 0);
Texture3D<float4> g_HistoryImage : REGISTER_SRV(HISTORY_IMAGE_BINDING, 0);
SamplerState g_LinearSampler : REGISTER_SAMPLER(LINEAR_SAMPLER_BINDING, 0);
ConstantBuffer<Constants> g_Constants : REGISTER_CBV(CONSTANT_BUFFER_BINDING, 0);
ByteAddressBuffer g_ExposureData : REGISTER_SRV(EXPOSURE_DATA_BUFFER_BINDING, 0);


float3 calcWorldSpacePos(float3 texelCoord)
{
	uint3 imageDims;
	g_ResultImage.GetDimensions(imageDims.x, imageDims.y, imageDims.z);
	float2 uv = texelCoord.xy / float2(imageDims.xy);
	
	float3 pos = lerp(g_Constants.frustumCornerTL, g_Constants.frustumCornerTR, uv.x);
	pos = lerp(pos, lerp(g_Constants.frustumCornerBL, g_Constants.frustumCornerBR, uv.x), uv.y);
	
	//pos = normalize(pos);
	
	float d = texelCoord.z * (1.0 / VOLUME_DEPTH);
	float z = VOLUME_NEAR * exp2(d * (log2(VOLUME_FAR / VOLUME_NEAR)));
	pos *= z / VOLUME_FAR;
	
	pos += g_Constants.cameraPos;
	
	return pos;
}

[numthreads(4, 4, 4)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	float4 result = 0.0;
	bool noCurrentData = (((threadID.x + threadID.y) & 1) == g_Constants.checkerBoardCondition);
	noCurrentData = (threadID.z & 1) ? !noCurrentData : noCurrentData;
	if (!noCurrentData)
	{
		result = g_InputImage.Load(uint4(threadID.xy, threadID.z / 2, 0));
	}
	
	// reproject and combine with previous result from previous frame
	if (g_Constants.ignoreHistory == 0)
	{
		float4 prevViewSpacePos = mul(g_Constants.prevViewMatrix, float4(calcWorldSpacePos(threadID + 0.5), 1.0));
		
		float z = -prevViewSpacePos.z;//length(prevViewSpacePos.xyz);
		float d = (log2(max(0, z * (1.0 / VOLUME_NEAR))) * (1.0 / log2(VOLUME_FAR / VOLUME_NEAR)));

		float4 prevClipSpacePos = mul(g_Constants.prevProjMatrix, prevViewSpacePos);
		float3 prevTexCoord = float3((prevClipSpacePos.xy / prevClipSpacePos.w) * float2(0.5, -0.5) + 0.5, d);
		//prevTexCoord.xy = prevTexCoord.xy * g_Constants.reprojectedTexCoordScaleBias.xy + g_Constants.reprojectedTexCoordScaleBias.zw;
		
		bool validCoord = all(prevTexCoord >= 0.0 && prevTexCoord <= 1.0);
		float4 prevResult = 0.0;
		if (validCoord)
		{
			prevResult = g_HistoryImage.SampleLevel(g_LinearSampler, prevTexCoord, 0.0);
			
			// prevResult.rgb is pre-exposed -> convert from previous frame exposure to current frame exposure
			prevResult.rgb *= asfloat(g_ExposureData.Load(1 << 2)); // 0 = current frame exposure | 1 = previous frame to current frame exposure
		}
		
		result = noCurrentData ? prevResult : result;
		result = lerp(prevResult, result, validCoord ? g_Constants.alpha : 1.0);
		if (!validCoord)
		{
			result = 0.0;
			result += g_InputImage.Load(uint4((int2)threadID.xy + int2(0, 1), threadID.z / 2, 0));
			result += g_InputImage.Load(uint4((int2)threadID.xy + int2(-1, 0), threadID.z / 2, 0));
			result += g_InputImage.Load(uint4((int2)threadID.xy + int2(1, 0), threadID.z / 2, 0));
			result += g_InputImage.Load(uint4((int2)threadID.xy + int2(0, -1), threadID.z / 2, 0));
			result *= 0.25;
		}
	}
	
	g_ResultImage[threadID] = result;
}
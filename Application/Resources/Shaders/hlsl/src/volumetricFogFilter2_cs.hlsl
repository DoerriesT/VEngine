#include "bindingHelper.hlsli"
#include "volumetricFogFilter2.hlsli"
#include "common.hlsli"


RWTexture3D<float4> g_ResultImage : REGISTER_UAV(RESULT_IMAGE_BINDING, 0);
Texture3D<float4> g_InputImage : REGISTER_SRV(INPUT_IMAGE_BINDING, 0);
Texture3D<float4> g_HistoryImage : REGISTER_SRV(HISTORY_IMAGE_BINDING, 0);
ConstantBuffer<Constants> g_Constants : REGISTER_CBV(CONSTANT_BUFFER_BINDING, 0);
ByteAddressBuffer g_ExposureData : REGISTER_SRV(EXPOSURE_DATA_BUFFER_BINDING, 0);

SamplerState g_Samplers[SAMPLER_COUNT] : REGISTER_SAMPLER(0, 1);

float3 calcWorldSpacePos(float3 texelCoord)
{
	uint3 imageDims;
	g_ResultImage.GetDimensions(imageDims.x, imageDims.y, imageDims.z);
	float2 uv = texelCoord.xy / float2(imageDims.xy);
	
	float3 pos = lerp(g_Constants.frustumCornerTL, g_Constants.frustumCornerTR, uv.x);
	pos = lerp(pos, lerp(g_Constants.frustumCornerBL, g_Constants.frustumCornerBR, uv.x), uv.y);
	
	float d = texelCoord.z * rcp(g_Constants.volumeDepth);
	float z = g_Constants.volumeNear * exp2(d * (log2(g_Constants.volumeFar / g_Constants.volumeNear)));
	pos *= z / g_Constants.volumeFar;
	
	pos += g_Constants.cameraPos;
	
	return pos;
}

[numthreads(4, 4, 4)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	float4 result = 0.0;
	bool checkerboardHole = (((threadID.x + threadID.y) & 1) == g_Constants.checkerBoardCondition);
	checkerboardHole = (threadID.z & 1) ? !checkerboardHole : checkerboardHole;
	if (!checkerboardHole)
	{
		result = g_InputImage.Load(uint4(threadID.xy, threadID.z / 2, 0));
	}
	
	// reproject and combine with previous result from previous frame
	if (g_Constants.ignoreHistory == 0)
	{
		float4 prevViewSpacePos = mul(g_Constants.prevViewMatrix, float4(calcWorldSpacePos(threadID + 0.5), 1.0));
		
		float z = -prevViewSpacePos.z;//length(prevViewSpacePos.xyz);
		float d = log2(max(0, z * rcp(g_Constants.volumeNear))) * rcp(log2(g_Constants.volumeFar / g_Constants.volumeNear));

		float4 prevClipSpacePos = mul(g_Constants.prevProjMatrix, prevViewSpacePos);
		float3 prevTexCoord = float3((prevClipSpacePos.xy / prevClipSpacePos.w) * float2(0.5, -0.5) + 0.5, d);
		
		bool validCoord = all(prevTexCoord >= 0.0 && prevTexCoord <= 1.0);
		float4 prevResult = 0.0;
		if (validCoord)
		{
			prevResult = g_HistoryImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], prevTexCoord, 0.0);
			
			// prevResult.rgb is pre-exposed -> convert from previous frame exposure to current frame exposure
			prevResult.rgb *= asfloat(g_ExposureData.Load(1 << 2)); // 0 = current frame exposure | 1 = previous frame to current frame exposure
			
			result = checkerboardHole ? prevResult : lerp(prevResult, result, g_Constants.alpha);
		}
		
		if (!validCoord && checkerboardHole)
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
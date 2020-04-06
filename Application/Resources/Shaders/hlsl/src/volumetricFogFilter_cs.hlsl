#include "bindingHelper.hlsli"
#include "volumetricFogFilter.hlsli"


#define VOLUME_DEPTH (64)
#define VOLUME_NEAR (0.5)
#define VOLUME_FAR (64.0)

RWTexture3D<float4> g_ResultImage : REGISTER_UAV(RESULT_IMAGE_BINDING, RESULT_IMAGE_SET);
Texture3D<float4> g_InputImage : REGISTER_SRV(INPUT_IMAGE_BINDING, INPUT_IMAGE_SET);
Texture3D<float4> g_HistoryImage : REGISTER_SRV(HISTORY_IMAGE_BINDING, HISTORY_IMAGE_SET);
SamplerState g_LinearSampler : REGISTER_SAMPLER(LINEAR_SAMPLER_BINDING, LINEAR_SAMPLER_SET);
ConstantBuffer<Constants> g_Constants : REGISTER_CBV(CONSTANT_BUFFER_BINDING, CONSTANT_BUFFER_SET);

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

[numthreads(2, 2, 16)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	uint3 froxelID = threadID.xyz;
	
	float4 result = g_InputImage.Load(int4(froxelID, 0));
	
	// reproject and combine with previous result from previous frame
	{
		float4 prevViewSpacePos = mul(g_Constants.prevViewMatrix, float4(calcWorldSpacePos(froxelID + 0.5), 1.0));
		
		float z = -prevViewSpacePos.z;
		float d = (log2(max(0, z * (1.0 / VOLUME_NEAR))) * (1.0 / log2(VOLUME_FAR / VOLUME_NEAR)));

		float4 prevClipSpacePos = mul(g_Constants.prevProjMatrix, prevViewSpacePos);
		float3 prevTexCoord = float3((prevClipSpacePos.xy / prevClipSpacePos.w) * 0.5 + 0.5, d);
		prevTexCoord.xy = prevTexCoord.xy * g_Constants.reprojectedTexCoordScaleBias.xy + g_Constants.reprojectedTexCoordScaleBias.zw;
		
		bool validCoord = all(prevTexCoord >= 0.0 && prevTexCoord <= 1.0);
		float4 prevResult = validCoord ? g_HistoryImage.SampleLevel(g_LinearSampler, prevTexCoord, 0.0) : 0.0;
		
		result = lerp(prevResult, result, validCoord ? 0.015 : 1.0);
	}
	
	g_ResultImage[froxelID] = result;
}
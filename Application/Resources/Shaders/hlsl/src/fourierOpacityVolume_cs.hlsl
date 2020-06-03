#include "bindingHelper.hlsli"
#include "fourierOpacityVolume.hlsli"
#include "common.hlsli"

RWTexture2D<float4> g_Result0Image : REGISTER_UAV(RESULT_0_IMAGE_BINDING, 0);
RWTexture2D<float4> g_Result1Image : REGISTER_UAV(RESULT_1_IMAGE_BINDING, 0);
StructuredBuffer<LightInfo> g_LightInfo : REGISTER_SRV(LIGHT_INFO_BINDING, 0);
StructuredBuffer<GlobalParticipatingMedium> g_GlobalMedia : REGISTER_SRV(GLOBAL_MEDIA_BINDING, 0);
StructuredBuffer<LocalParticipatingMedium> g_LocalMedia : REGISTER_SRV(LOCAL_MEDIA_BINDING, 0);

PUSH_CONSTS(PushConsts, g_PushConsts);

groupshared float4 s_result0[2];
groupshared float4 s_result1[2];

[numthreads(64, 1, 1)]
void main(uint3 threadID : SV_DispatchThreadID, uint3 groupID : SV_GroupID, uint groupThreadIndex : SV_GroupIndex)
{
	const uint2 pixelCoord = groupID.xy;
	const uint depthCoord = groupThreadIndex;
	const LightInfo lightInfo = g_LightInfo[g_PushConsts.lightIndex];
	
	// compute ray from light source passing through the current pixel
	const float2 texCoord = (groupID.xy + 0.5) * g_PushConsts.texelSize;
	const float4 farPlaneWorldSpacePos = mul(lightInfo.invViewProjection, float4(texCoord * 2.0 - 1.0, 1.0, 1.0));
	float3 ray = (farPlaneWorldSpacePos.xyz / farPlaneWorldSpacePos.w) - lightInfo.position;
	const float rayLen = length(ray);
	ray *= rcp(rayLen);
	
	// ray segment of current thread
	const float segmentLength = rayLen / 64.0;
	const float rayStart = segmentLength * depthCoord;
	const float rayEnd = segmentLength * (depthCoord + 1);
	
	float4 result0 = 0.0;
	float4 result1 = 0.0;
	
	// march ray
	//const float stepSize = 0.1;
	//const int stepCount = (int)ceil((rayEnd - rayStart) / stepSize);
	const int stepCount = clamp(ceil(segmentLength / 0.05), 1, 10);
	const float stepSize = (rayEnd - rayStart) / stepCount;
	for (int i = 0; i < stepCount; ++i)
	{
		float t = i * stepSize + rayStart;
		float3 rayPos = lightInfo.position + ray * t;
		
		float extinction = 0.0;
		
		// add extinction of all global volumes
		{
			for (int j = 0; j < g_PushConsts.globalMediaCount; ++j)
			{
				GlobalParticipatingMedium medium = g_GlobalMedia[j];
				extinction += medium.extinction;
			}
		}
		
		// add extinction of all local volumes
		for (int j = 0; j < g_PushConsts.localVolumeCount; ++j)
		{
			LocalParticipatingMedium medium = g_LocalMedia[j];
			
			const float3 localPos = float3(dot(medium.worldToLocal0, float4(rayPos, 1.0)), 
									dot(medium.worldToLocal1, float4(rayPos, 1.0)), 
									dot(medium.worldToLocal2, float4(rayPos, 1.0)));
									
			if (all(abs(localPos) <= 1.0))
			{
				extinction += medium.extinction;
			}
		}
		
		float transmittance = exp(-extinction * stepSize);
		float depth = t * lightInfo.depthScale + lightInfo.depthBias;
		
		result0.r += -2.0 * log(transmittance) * cos(2.0 * PI * 0.0 * depth);
		result0.g += -2.0 * log(transmittance) * sin(2.0 * PI * 0.0 * depth);
		
		result0.b += -2.0 * log(transmittance) * cos(2.0 * PI * 1.0 * depth);
		result0.a += -2.0 * log(transmittance) * sin(2.0 * PI * 1.0 * depth);
		
		result1.r += -2.0 * log(transmittance) * cos(2.0 * PI * 2.0 * depth);
		result1.g += -2.0 * log(transmittance) * sin(2.0 * PI * 2.0 * depth);
		
		result1.b += -2.0 * log(transmittance) * cos(2.0 * PI * 3.0 * depth);
		result1.a += -2.0 * log(transmittance) * sin(2.0 * PI * 3.0 * depth);
	}
	
	//s_result0[groupThreadIndex] = result0;
	//s_result1[groupThreadIndex] = result1;
	//
	//GroupMemoryBarrierWithGroupSync();
	//
	//if (groupThreadIndex == 0)
	//{
	//	float4 r0 = 0.0;
	//	float4 r1 = 0.0;
	//	for (int i = 0; i < 64; ++i)
	//	{
	//		r0 += s_result0[i];
	//		r1 += s_result1[i];
	//	}
	//	g_Result0Image[pixelCoord] = r0;
	//	g_Result1Image[pixelCoord] = r1;
	//}
	
	// sum all results
	result0 = WaveActiveSum(result0);
	result1 = WaveActiveSum(result1);
	
	const uint waveSize = WaveGetLaneCount();
	
	// entire group fits into a single wave: no need for LDS; wave intrinsics already added every result
	if (waveSize >= 64)
	{
		if (WaveIsFirstLane())
		{
			g_Result0Image[pixelCoord] = result0;
			g_Result1Image[pixelCoord] = result1;
		}
		return;
	}
	
	// add final result via LDS
	if (WaveIsFirstLane() && waveSize < 64)
	{
		int waveIndex = groupThreadIndex / waveSize;
		s_result0[waveIndex] = result0;
		s_result1[waveIndex] = result1;
	}
	
	GroupMemoryBarrierWithGroupSync();
	
	if (groupThreadIndex == 0)
	{
		g_Result0Image[pixelCoord] = s_result0[0] + s_result0[1];
		g_Result1Image[pixelCoord] = s_result1[0] + s_result1[1];
	}
}
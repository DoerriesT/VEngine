#include "bindingHelper.hlsli"
#include "fourierOpacityVolume.hlsli"
#include "common.hlsli"
#include "commonEncoding.hlsli"

RWTexture2D<float4> g_Result0Image : REGISTER_UAV(RESULT_0_IMAGE_BINDING, 0);
RWTexture2D<float4> g_Result1Image : REGISTER_UAV(RESULT_1_IMAGE_BINDING, 0);
StructuredBuffer<LightInfo> g_LightInfo : REGISTER_SRV(LIGHT_INFO_BINDING, 0);
StructuredBuffer<GlobalParticipatingMedium> g_GlobalMedia : REGISTER_SRV(GLOBAL_MEDIA_BINDING, 0);
StructuredBuffer<LocalParticipatingMedium> g_LocalMedia : REGISTER_SRV(LOCAL_MEDIA_BINDING, 0);

PUSH_CONSTS(PushConsts, g_PushConsts);

groupshared float4 s_result0[2];
groupshared float4 s_result1[2];

void writeResult(uint2 dstCoord, uint2 localOffset, uint resolution, bool octahedronMap, float4 result0, float4 result1)
{
	g_Result0Image[dstCoord] = result0;
	g_Result1Image[dstCoord] = result1;
	
	// replicate border texels in outer "gutter" region to ensure correct linear sampling
	if (octahedronMap)
	{
		uint2 localCoord = dstCoord - localOffset;
		const bool4 border = bool4(localCoord.x == 0, localCoord.y == 0, localCoord.x == (resolution - 1), localCoord.y == (resolution - 1));
		
		const int2 offset = int2(localCoord) * -2 + (int(resolution) - 1);
		
		// horizontal border
		if (border.x || border.z)
		{
			const int2 horizontalBorderCoord = border.x ? (dstCoord + int2(-1, offset.y)) : (dstCoord + int2(1, offset.y));
			g_Result0Image[horizontalBorderCoord] = result0;
			g_Result1Image[horizontalBorderCoord] = result1;
		}
		
		// vertical border
		if (border.y || border.w)
		{
			const int2 verticalBorderCoord = border.y ? (dstCoord + int2(offset.x, -1)) : dstCoord + int2(offset.x, 1);
			g_Result0Image[verticalBorderCoord] = result0;
			g_Result1Image[verticalBorderCoord] = result1;
		}
		
		// corner
		if ((border.x && border.y) || (border.x && border.w) || (border.z && border.y) || (border.z && border.w))
		{
			const int2 cornerCoord = dstCoord + int2(border.xy ? int2(resolution, resolution) : int2(-resolution, -resolution));
			g_Result0Image[cornerCoord] = result0;
			g_Result1Image[cornerCoord] = result1;
		}
	}
}

[numthreads(64, 1, 1)]
void main(uint3 threadID : SV_DispatchThreadID, uint3 groupID : SV_GroupID, uint groupThreadIndex : SV_GroupIndex)
{
	const LightInfo lightInfo = g_LightInfo[g_PushConsts.lightIndex];
	uint2 dstCoord = groupID.xy + uint2(lightInfo.offsetX, lightInfo.offsetY);
	const float2 texCoord = (groupID.xy + 0.5) * lightInfo.texelSize;
	const uint depthCoord = groupThreadIndex;
	
	// compute ray from light source passing through the current pixel
	float3 ray = 0.0;
	if (lightInfo.isPointLight)
	{
		ray = -decodeOctahedron(texCoord * 2.0 - 1.0);
	}
	else
	{
		const float4 farPlaneWorldSpacePos = mul(lightInfo.invViewProjection, float4(texCoord * 2.0 - 1.0, 1.0, 1.0));
		ray = normalize((farPlaneWorldSpacePos.xyz / farPlaneWorldSpacePos.w) - lightInfo.position);
	}
	
	// ray segment of current thread
	const float segmentLength = lightInfo.radius / 64.0;
	const float rayStart = segmentLength * depthCoord;
	const float rayEnd = segmentLength * (depthCoord + 1);
	const float invRadius = 1.0 / lightInfo.radius;
	
	float4 result0 = 0.0;
	float4 result1 = 0.0;
	
	// march ray
	//const float stepSize = 0.1;
	//const int stepCount = (int)ceil((rayEnd - rayStart) / stepSize);
	const int stepCount = clamp(ceil(segmentLength / 0.05), 1, 10);
	const float stepSize = (rayEnd - rayStart) / stepCount;
	
	//float2x2 ditherMatrix = float2x2(0.25, 0.75, 1.0, 0.5);
	//float dither = ditherMatrix[groupID.y % 2][groupID.x % 2] * stepSize;
	
	float4x4 ditherMatrix = float4x4(1 / 16.0, 9 / 16.0, 3 / 16.0, 11 / 16.0,
									13 / 16.0, 5 / 16.0, 15 / 16.0, 7 / 16.0,
									4 / 16.0, 12 / 16.0, 2 / 16.0, 10 / 16.0,
									16 / 16.0, 8 / 16.0, 14 / 16.0, 6 / 16.0);
	float dither = ditherMatrix[groupID.y % 4][groupID.x % 4] * stepSize;
	
	for (int i = 0; i < stepCount; ++i)
	{
		float t = i * stepSize + dither + rayStart;
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
		float depth = t * invRadius;
		
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
	//	writeResult(dstCoord, uint2(lightInfo.offsetX, lightInfo.offsetY), lightInfo.resolution, (bool)lightInfo.isPointLight, r0, r1);
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
			writeResult(dstCoord, uint2(lightInfo.offsetX, lightInfo.offsetY), lightInfo.resolution, (bool)lightInfo.isPointLight, result0, result1);
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
		result0 = s_result0[0] + s_result0[1];
		result1 = s_result1[0] + s_result1[1];
		writeResult(dstCoord, uint2(lightInfo.offsetX, lightInfo.offsetY), lightInfo.resolution, (bool)lightInfo.isPointLight, result0, result1);
	}
}
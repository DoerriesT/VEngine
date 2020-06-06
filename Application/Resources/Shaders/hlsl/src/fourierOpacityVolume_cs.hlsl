#include "bindingHelper.hlsli"
#include "fourierOpacityVolume.hlsli"
#include "common.hlsli"
#include "commonEncoding.hlsli"
#include "commonFourierOpacity.hlsli"

RWTexture2D<float4> g_Result0Image : REGISTER_UAV(RESULT_0_IMAGE_BINDING, 0);
RWTexture2D<float4> g_Result1Image : REGISTER_UAV(RESULT_1_IMAGE_BINDING, 0);
StructuredBuffer<LightInfo> g_LightInfo : REGISTER_SRV(LIGHT_INFO_BINDING, 0);
StructuredBuffer<GlobalParticipatingMedium> g_GlobalMedia : REGISTER_SRV(GLOBAL_MEDIA_BINDING, 0);
StructuredBuffer<LocalParticipatingMedium> g_LocalMedia : REGISTER_SRV(LOCAL_MEDIA_BINDING, 0);

PUSH_CONSTS(PushConsts, g_PushConsts);

groupshared float4 s_result0[64];
groupshared float4 s_result1[64];

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
void main(uint3 threadID : SV_DispatchThreadID, uint3 groupID : SV_GroupID, uint groupThreadIndex : SV_GroupIndex, uint3 groupThreadID : SV_GroupThreadID)
{
	uint rayID = groupThreadID.x / 4;
	uint3 localCoord;
	localCoord.y = groupThreadID.x / 16;
	localCoord.z = groupThreadID.x - rayID * 4;
	localCoord.x = rayID - localCoord.y * 4;
	
	uint2 globalCoord = groupID.xy * 4 + localCoord.xy;
	
	
	const LightInfo lightInfo = g_LightInfo[g_PushConsts.lightIndex];
	if (globalCoord.x >= lightInfo.resolution || globalCoord.y >= lightInfo.resolution)
	{
		s_result0[groupThreadID.x] = 0.0;
		s_result1[groupThreadID.x] = 0.0;
		return;
	}
	
	uint2 dstCoord = globalCoord.xy + uint2(lightInfo.offsetX, lightInfo.offsetY);
	const float2 texCoord = (globalCoord.xy + 0.5) * lightInfo.texelSize;
	const uint depthCoord = localCoord.z;
	
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
	const float segmentLength = lightInfo.radius / 4.0;
	float rayStart = segmentLength * depthCoord;
	const float rayEnd = segmentLength * (depthCoord + 1);
	const float invRadius = 1.0 / lightInfo.radius;
	
	float4 result0 = 0.0;
	float4 result1 = 0.0;
	
	// march ray
	//const float stepSize = 0.1;
	//const int stepCount = (int)ceil((rayEnd - rayStart) / stepSize);
	const int stepCount = clamp(ceil(segmentLength / 0.05), 1, 64);
	const float stepSize = (rayEnd - rayStart) / stepCount;
	
	//float2x2 ditherMatrix = float2x2(0.25, 0.75, 1.0, 0.5);
	//float dither = ditherMatrix[groupID.y % 2][groupID.x % 2] * stepSize;
	
	float4x4 ditherMatrix = float4x4(1 / 16.0, 9 / 16.0, 3 / 16.0, 11 / 16.0,
									13 / 16.0, 5 / 16.0, 15 / 16.0, 7 / 16.0,
									4 / 16.0, 12 / 16.0, 2 / 16.0, 10 / 16.0,
									16 / 16.0, 8 / 16.0, 14 / 16.0, 6 / 16.0);
	float dither = ditherMatrix[globalCoord.y % 4][globalCoord.x % 4] * stepSize;
	
	rayStart += dither + stepSize * 0.5;
	
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
		float depth = t * invRadius;
		fourierOpacityAccumulate(depth, transmittance, result0, result1);
	}
	
	if (localCoord.z != 0)
	{
		s_result0[groupThreadID.x] = result0;
		s_result1[groupThreadID.x] = result1;
	}
	
	GroupMemoryBarrierWithGroupSync();
	
	if (localCoord.z == 0)
	{
		for (int i = 1; i < 4; ++i)
		{
			result0 += s_result0[groupThreadID.x + i];
			result1 += s_result1[groupThreadID.x + i];
		}
		writeResult(dstCoord, uint2(lightInfo.offsetX, lightInfo.offsetY), lightInfo.resolution, (bool)lightInfo.isPointLight, result0, result1);
	}
}
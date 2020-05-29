#include "bindingHelper.hlsli"
#include "common.hlsli"
#include "volumetricFogExtinctionVolume.hlsli"

RWTexture3D<float> g_ExtinctionImage : REGISTER_UAV(EXTINCTION_IMAGE_BINDING, 0);
StructuredBuffer<GlobalParticipatingMedium> g_GlobalMedia : REGISTER_SRV(GLOBAL_MEDIA_BINDING, 0);
StructuredBuffer<LocalParticipatingMedium> g_LocalMedia : REGISTER_SRV(LOCAL_MEDIA_BINDING, 0);

PUSH_CONSTS(PushConsts, g_PushConsts);

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	float extinctionSum = 0.0;
	
	// iterate over all global participating media
	{
		for (int i = 0; i < g_PushConsts.globalMediaCount; ++i)
		{
			GlobalParticipatingMedium medium = g_GlobalMedia[i];
			extinctionSum += medium.extinction;
		}
	}
	
	float3 worldSpacePos = (float3)(threadID + 0.5) * g_PushConsts.positionScale + g_PushConsts.positionBias;

	// iterate over all local participating media
	{
		for (int i = 0; i < g_PushConsts.localMediaCount; ++i)
		{
			LocalParticipatingMedium medium = g_LocalMedia[i];
			
			const float3 localPos = float3(dot(medium.worldToLocal0, float4(worldSpacePos, 1.0)), 
									dot(medium.worldToLocal1, float4(worldSpacePos, 1.0)), 
									dot(medium.worldToLocal2, float4(worldSpacePos, 1.0)));
									
			if (all(abs(localPos) <= 1.0))
			{
				extinctionSum += medium.extinction;
			}
		}
	}

	g_ExtinctionImage[threadID + int3(0, 0, g_PushConsts.dstOffset)] = extinctionSum;
}
#include "bindingHelper.hlsli"
#include "volumetricFogVBuffer.hlsli"
#include "packing.hlsli"
#include "common.hlsli"
#include "lighting.hlsli"

#define VOLUME_DEPTH (64)
#define VOLUME_NEAR (0.5)
#define VOLUME_FAR (64.0)

RWTexture3D<float4> g_ScatteringExtinctionImage : REGISTER_UAV(SCATTERING_EXTINCTION_IMAGE_BINDING, SCATTERING_EXTINCTION_IMAGE_SET);
RWTexture3D<float4> g_EmissivePhaseImage : REGISTER_UAV(EMISSIVE_PHASE_IMAGE_BINDING, EMISSIVE_PHASE_IMAGE_SET);
ConstantBuffer<Constants> g_Constants : REGISTER_CBV(CONSTANT_BUFFER_BINDING, CONSTANT_BUFFER_SET);
StructuredBuffer<GlobalParticipatingMedium> g_GlobalMedia : REGISTER_SRV(GLOBAL_MEDIA_BINDING, GLOBAL_MEDIA_SET);

// local media
StructuredBuffer<LocalParticipatingMedium> g_LocalMedia : REGISTER_SRV(LOCAL_MEDIA_BINDING, LOCAL_MEDIA_SET);
ByteAddressBuffer g_LocalMediaBitMask : REGISTER_SRV(LOCAL_MEDIA_BIT_MASK_BINDING, LOCAL_MEDIA_BIT_MASK_SET);
ByteAddressBuffer g_LocalMediaDepthBins : REGISTER_SRV(LOCAL_MEDIA_Z_BINS_BINDING, LOCAL_MEDIA_Z_BINS_SET);

//PUSH_CONSTS(PushConsts, g_PushConsts);

float3 calcWorldSpacePos(float3 texelCoord)
{
	uint3 imageDims;
	g_ScatteringExtinctionImage.GetDimensions(imageDims.x, imageDims.y, imageDims.z);
	float2 uv = texelCoord.xy / float2(imageDims.xy);
	
	float3 pos = lerp(g_Constants.frustumCornerTL, g_Constants.frustumCornerTR, uv.x);
	pos = lerp(pos, lerp(g_Constants.frustumCornerBL, g_Constants.frustumCornerBR, uv.x), uv.y);
	
	float d = texelCoord.z * (1.0 / VOLUME_DEPTH);
	float z = VOLUME_NEAR * exp2(d * (log2(VOLUME_FAR / VOLUME_NEAR)));
	pos *= z / VOLUME_FAR;
	
	pos += g_Constants.cameraPos;
	
	return pos;
}

[numthreads(2, 2, 16)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	//float4x4 ditherMatrix = ((float4x4(0, 8, 2, 10,
	//								12, 4, 14, 6,
	//								3, 11, 1, 9,
	//								15, 7, 13, 5) + 1.0) * (1.0 / 16.0));
	//float dither = ditherMatrix[threadID.y % 4][threadID.x % 4];
	
	float2x2 ditherMatrix = float2x2(0.25, 0.75, 1.0, 0.5);
	float dither = ditherMatrix[threadID.y % 2][threadID.x % 2];
	
	const float3 worldSpacePos0 = calcWorldSpacePos(threadID + float3(g_Constants.jitterX, g_Constants.jitterY, frac(g_Constants.jitterZ + dither)));
	const float3 viewSpacePos0 = mul(g_Constants.viewMatrix, float4(worldSpacePos0, 1.0)).xyz;
	
	const float3 worldSpacePos1 = calcWorldSpacePos(threadID + float3(g_Constants.jitter1.x, g_Constants.jitter1.y, frac(g_Constants.jitter1.z + dither)));
	const float3 viewSpacePos1 = mul(g_Constants.viewMatrix, float4(worldSpacePos1, 1.0)).xyz;

	const float3 viewSpacePos[] = { viewSpacePos0, viewSpacePos1 };

	uint3 imageDims;
	g_ScatteringExtinctionImage.GetDimensions(imageDims.x, imageDims.y, imageDims.z);
	uint targetImageWidth = imageDims.x * 8;
	
	float3 scattering = 0.0;
	float extinction = 0.0;
	float3 emissive = 0.0;
	float phase = 0.0;
	uint accumulatedMediaCount = 0;
	
	// iterate over all global participating media
	{
		for (int i = 0; i < g_Constants.globalMediaCount; ++i)
		{
			GlobalParticipatingMedium medium = g_GlobalMedia[i];
			scattering += medium.scattering;
			extinction += medium.extinction;
			emissive += medium.emissive;
			phase += medium.phase;
			++accumulatedMediaCount;
		}
	}

	// iterate over all local participating media
	uint localMediaCount = g_Constants.localMediaCount;
	if (localMediaCount > 0)
	{
		uint wordMin, wordMax, minIndex, maxIndex, wordCount;
		getLightingMinMaxIndicesRange(g_LocalMediaDepthBins, localMediaCount, -viewSpacePos[0].z, -viewSpacePos[1].z, minIndex, maxIndex, wordMin, wordMax, wordCount);
		const uint address = getTileAddress(threadID.xy * 8, targetImageWidth, wordCount);
	
		for (uint wordIndex = wordMin; wordIndex <= wordMax; ++wordIndex)
		{
			uint mask = getLightingBitMask(g_LocalMediaBitMask, address, wordIndex, minIndex, maxIndex);
			
			while (mask != 0)
			{
				const uint bitIndex = firstbitlow(mask);
				const uint index = 32 * wordIndex + bitIndex;
				mask ^= (1 << bitIndex);
				
				LocalParticipatingMedium medium = g_LocalMedia[index];
				
				[unroll]
				for (int i = 0; i < 2; ++i)
				{
					float3 localPos = mul(viewSpacePos[i] - medium.position, float3x3(medium.obbAxis0, medium.obbAxis1, medium.obbAxis2));
				
					bool insideMedium = all(abs(localPos) <= float3(medium.extentX, medium.extentY, medium.extentZ));
					
					if (insideMedium)
					{
						scattering += medium.scattering;
						extinction += medium.extinction;
						emissive += medium.emissive;
						phase += medium.phase;
						++accumulatedMediaCount;
					}
				}
			}
		}
	}
	
	phase = accumulatedMediaCount > 0 ? phase * rcp((float)accumulatedMediaCount) : 0.0;
	
	g_ScatteringExtinctionImage[threadID] = float4(scattering, extinction);
	g_EmissivePhaseImage[threadID] = float4(emissive, phase);
}
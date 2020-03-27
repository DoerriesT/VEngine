#include "bindingHelper.hlsli"
#include "luminanceHistogram.hlsli"
#include "common.hlsli"

#define LOCAL_SIZE_X 64

Texture2D<float4> g_InputImage : REGISTER_SRV(INPUT_IMAGE_BINDING, INPUT_IMAGE_SET);
RWByteAddressBuffer g_LuminanceHistogram : REGISTER_UAV(LUMINANCE_HISTOGRAM_BINDING, LUMINANCE_HISTOGRAM_SET);

PUSH_CONSTS(PushConsts, g_PushConsts);

groupshared uint s_localHistogram[LUMINANCE_HISTOGRAM_SIZE];

[numthreads(LOCAL_SIZE_X, 1, 1)]
void main(uint3 groupThreadID : SV_GroupThreadID, uint3 groupID : SV_GroupID)
{
	// clear local histogram
	{
		for (uint i = groupThreadID.x; i < LUMINANCE_HISTOGRAM_SIZE; i += LOCAL_SIZE_X)
		{
			s_localHistogram[i] = 0;
		}
	}
	
	GroupMemoryBarrierWithGroupSync();
	
	// each work group computes a local histogram for one horizontal line in the input image
	{
		const uint width = g_PushConsts.width;
		for (uint i = groupThreadID.x; i < width; i += LOCAL_SIZE_X)
		{
			if (i < width)
			{
				float3 color = g_InputImage.Load(int3(i, groupID.x, 0)).rgb;
				float luma = dot(color, float3(0.2126, 0.7152, 0.0722));
			
				luma = log2(max(luma, 1e-5)) * g_PushConsts.scale + g_PushConsts.bias;//log(luma + 1.0) * 128;
				
				uint bucket = uint(saturate(luma) * (LUMINANCE_HISTOGRAM_SIZE - 1));//min(uint(luma), 255);
				
				InterlockedAdd(s_localHistogram[bucket], 1);
			}
		}
	}
	
	GroupMemoryBarrierWithGroupSync();
	
	// merge with global histogram
	{
		for (uint i = groupThreadID.x; i < LUMINANCE_HISTOGRAM_SIZE; i += LOCAL_SIZE_X)
		{
			g_LuminanceHistogram.InterlockedAdd(i << 2, s_localHistogram[i]);
		}
	}
}
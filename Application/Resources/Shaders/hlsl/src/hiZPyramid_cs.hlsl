#include "bindingHelper.hlsli"
#include "hiZPyramid.hlsli"

#ifndef REDUCE
#define REDUCE min
#endif // REDUCE

RWTexture2D<float> g_ResultImage : REGISTER_UAV(RESULT_IMAGE_BINDING, RESULT_IMAGE_SET);
Texture2D<float> g_InputImage : REGISTER_SRV(INPUT_IMAGE_BINDING, INPUT_IMAGE_SET);

PUSH_CONSTS(PushConsts, g_PushConsts);

[numthreads(16, 16, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	if (threadID.x >= g_PushConsts.width || threadID.y >= g_PushConsts.height)
	{
		return;
	}
	
	if (g_PushConsts.copyOnly != 0)
	{
		float depth = g_InputImage.Load(int3(threadID.xy, 0)).x;
		g_ResultImage[threadID.xy] = depth;
		return;
	}
	// get integer pixel coordinates
	int2 coords = int2(threadID.xy) * 2;
	
	// fetch a 2x2 neighborhood and compute the furthest depth
	float4 depths;
	depths[0] = g_InputImage.Load(int3(threadID.xy + int2(0, 0), 0)).x;
	depths[1] = g_InputImage.Load(int3(threadID.xy + int2(1, 0), 0)).x;
	depths[2] = g_InputImage.Load(int3(threadID.xy + int2(0, 1), 0)).x;
	depths[3] = g_InputImage.Load(int3(threadID.xy + int2(1, 1), 0)).x;
	
	float reducedDepth = REDUCE(REDUCE(depths[0], depths[1]), REDUCE(depths[2], depths[3]));
	
	const int2 prevMipSize = g_PushConsts.prevMipSize;
	
	// if we are reducing an odd-sized texture,
	// then the edge pixels need to fetch additional texels
	float2 extra;
	if (((prevMipSize.x & 1) == 1) && (coords.x == (prevMipSize.x - 3)))
	{
		extra[0] = g_InputImage.Load(int3(threadID.xy + int2(2, 0), 0)).x;
		extra[1] = g_InputImage.Load(int3(threadID.xy + int2(2, 1), 0)).x;
		reducedDepth = REDUCE(reducedDepth, REDUCE(extra[0], extra[1]));
	}
	
	if (((prevMipSize.y & 1) == 1) && (coords.y == (prevMipSize.y - 3)))
	{
		extra[0] = g_InputImage.Load(int3(threadID.xy + int2(0, 2), 0)).x;
		extra[1] = g_InputImage.Load(int3(threadID.xy + int2(1, 2), 0)).x;
		reducedDepth = REDUCE(reducedDepth, REDUCE(extra[0], extra[1]));
	}
	
	// extreme case: if both edges are odd, fetch the bottom-right corner
	if (((prevMipSize.x & 1) == 1) && ((prevMipSize.y & 1) == 1)
		&& (coords.x == (prevMipSize.x - 3)) && (coords.y == (prevMipSize.y - 3)))
	{
		reducedDepth = REDUCE(reducedDepth, g_InputImage.Load(int3(threadID.xy + int2(2, 2), 0)).x);
	}
	
	g_ResultImage[threadID.xy] = reducedDepth;
}
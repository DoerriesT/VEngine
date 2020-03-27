#include "bindingHelper.hlsli"
#include "exposure.hlsli"
#include "common.hlsli"

#define LOCAL_SIZE_X 64

ByteAddressBuffer g_LuminanceHistogram : REGISTER_SRV(LUMINANCE_HISTOGRAM_BINDING, LUMINANCE_HISTOGRAM_SET);
RWByteAddressBuffer g_LuminanceValues : REGISTER_UAV(LUMINANCE_VALUES_BINDING, LUMINANCE_VALUES_SET);

PUSH_CONSTS(PushConsts, g_PushConsts);

groupshared uint s_localHistogram[LUMINANCE_HISTOGRAM_SIZE];

[numthreads(LOCAL_SIZE_X, 1, 1)]
void main(uint3 groupThreadID : SV_GroupThreadID, uint3 groupID : SV_GroupID)
{
	// fill local histogram
	{
		for (uint i = groupThreadID.x; i < LUMINANCE_HISTOGRAM_SIZE; i += LOCAL_SIZE_X)
		{
			s_localHistogram[i] = g_LuminanceHistogram.Load(i << 2);
		}
	}
	
	GroupMemoryBarrierWithGroupSync();
	
	if (groupThreadID.x == 0)
	{
		float avgLuminance = 0.0;
		uint numProcessedPixels = 0;
		const uint lowerBound = g_PushConsts.lowerBound;
		const uint upperBound = g_PushConsts.upperBound;

		for (uint i = 0; i < LUMINANCE_HISTOGRAM_SIZE; ++i)
		{
			uint numPixels = s_localHistogram[i];
			
			uint tmpNumProcessedPixels = numProcessedPixels + numPixels;
			
			// subtract lower bound if it intersects this bucket
			numPixels -= min(lowerBound - min(numProcessedPixels, lowerBound), numPixels);
			// subtract upper bound if it intersects this bucket
			numPixels -= min(tmpNumProcessedPixels - min(upperBound, tmpNumProcessedPixels), numPixels);
			
			// get luma from bucket index
			//float luma = float(i) + 0.5;
			//luma *= (1.0 / 128.0);
			//luma = exp(luma);
			//luma -= 1.0;
			
			float luma = float(i) * (1.0 / (LUMINANCE_HISTOGRAM_SIZE - 1));
			luma = (luma - g_PushConsts.bias) * g_PushConsts.invScale;
			luma = exp2(luma);
			
			numProcessedPixels = tmpNumProcessedPixels;
			
			avgLuminance += numPixels * luma;
			
			if (numProcessedPixels >= upperBound)
			{
				break;
			}
		}
		
		avgLuminance /= max(upperBound - lowerBound, 1);
		
		float previousLum = asfloat(g_LuminanceValues.Load(g_PushConsts.previousResourceIndex << 2));
		
		bool adaptUpwards = avgLuminance >= previousLum;
	
		// Adapt the luminance using Pattanaik's technique
		float precomputedTerm = adaptUpwards ? g_PushConsts.precomputedTermUp : g_PushConsts.precomputedTermDown;
		g_LuminanceValues.Store(g_PushConsts.currentResourceIndex << 2, asuint(previousLum + (avgLuminance - previousLum) * precomputedTerm));
	}
}
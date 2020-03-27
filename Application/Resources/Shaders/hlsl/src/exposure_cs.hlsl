#include "bindingHelper.hlsli"
#include "exposure.hlsli"
#include "common.hlsli"

#define LOCAL_SIZE_X 64

ByteAddressBuffer g_LuminanceHistogram : REGISTER_SRV(LUMINANCE_HISTOGRAM_BINDING, LUMINANCE_HISTOGRAM_SET);
RWByteAddressBuffer g_LuminanceValues : REGISTER_UAV(LUMINANCE_VALUES_BINDING, LUMINANCE_VALUES_SET);
RWByteAddressBuffer g_ExposureData : REGISTER_UAV(EXPOSURE_DATA_BINDING, EXPOSURE_DATA_SET);

PUSH_CONSTS(PushConsts, g_PushConsts);

groupshared uint s_localHistogram[LUMINANCE_HISTOGRAM_SIZE];

float computeEV100(float aperture, float shutterTime, float ISO)
{
	// EV number is defined as:
	// 2^EV_s = N^2 / t and EV_s = EV_100 + log2(S/100)
	// This gives
	//   EV_s = log2(N^2 / t)
	//   EV_100 + log2(S/100) = log(N^2 / t)
	//   EV_100 = log2(N^2 / t) - log2(S/100)
	//   EV_100 = log2(N^2 / t * 100 / S)
	return log2((aperture * aperture) / shutterTime * 100 / ISO);
}

float computeEV100FromAvgLuminance(float avgLuminance)
{
	// We later use the middle gray at 12.7% in order to have
	// a middle gray at 12% with a sqrt(2) room for specular highlights
	// But here we deal with the spot meter measuring the middle gray
	// which is fixed at 12.5 for matching standard camera
	// constructor settings (i.e. calibration constant K = 12.5)
	// Reference: http://en.wikipedia.org/wiki/Film_speed
	return log2(max(avgLuminance, 1e-5) * 100.0 / 12.5);
}

float convertEV100ToExposure(float EV100)
{
	// Compute the maximum luminance possible with H_sbs sensitivity
	// maxLum = 78 / ( S * q ) * N^2 / t
	//        = 78 / ( S * q ) * 2^EV_100
	//        = 78 / ( 100 * 0.65) * 2^EV_100
	//        = 1.2 * 2^EV
	// Reference: http://en.wikipedia.org/wiki/Film_speed
	float maxLuminance = 1.2 * pow(2.0, EV100);
	return 1.0 / maxLuminance;
}

float computeExposureFromAvgLuminance(float avgLuminance)
{
	//float EV100 = computeEV100(16.0, 0.01, 100);
	float EV100 = computeEV100FromAvgLuminance(avgLuminance);
	EV100 -= g_PushConsts.exposureCompensation;
	float exposure = clamp(convertEV100ToExposure(EV100), g_PushConsts.exposureMin, g_PushConsts.exposureMax);
	return g_PushConsts.fixExposureToMax != 0 ? g_PushConsts.exposureMax : exposure;
}

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
		
		const float previousLum = asfloat(g_LuminanceValues.Load(g_PushConsts.previousResourceIndex << 2));
		
		const bool adaptUpwards = avgLuminance >= previousLum;
	
		// Adapt the luminance using Pattanaik's technique
		const float precomputedTerm = adaptUpwards ? g_PushConsts.precomputedTermUp : g_PushConsts.precomputedTermDown;
		const float lum = previousLum + (avgLuminance - previousLum) * precomputedTerm;
		
		g_LuminanceValues.Store(g_PushConsts.currentResourceIndex << 2, asuint(lum));
		
		const float previousExposure = computeExposureFromAvgLuminance(previousLum);
		const float exposure = computeExposureFromAvgLuminance(lum);
		const float previousToCurrent = exposure / previousExposure;
		
		g_ExposureData.Store(0, asuint(exposure));
		g_ExposureData.Store(4, asuint(previousToCurrent));
	}
}
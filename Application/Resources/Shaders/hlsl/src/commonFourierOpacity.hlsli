#ifndef COMMON_FOURIER_OPACITY
#define COMMON_FOURIER_OPACITY

#include "common.hlsli"

void fourierOpacityAccumulate(float depth, float transmittance, inout float4 result0, inout float4 result1)
{
	transmittance = max(transmittance, 1e-5);
	const float depthTwoPi = depth * 2.0 * PI;

	const float lnTransmittance = -2.0 * log(transmittance);

	// a = -2 * log(transmittance) * cos(2 * PI * k * depth)
	// b = -2 * log(transmittance) * sin(2 * PI * k * depth)

	result0.r += lnTransmittance;// * cos(depthTwoPi * 0.0); // cos(0.0) == 1.0
	//result0.g += lnTransmittance * sin(depthTwoPi * 0.0); // sin(0.0) == 0.0

	float sine, cosine;
	sincos(depthTwoPi, sine, cosine);
	result0.b += lnTransmittance * cosine;
	result0.a += lnTransmittance * sine;

	const float sine2 = sine * cosine + cosine * sine;
	const float cosine2 = cosine * cosine - sine * sine;
	result1.r += lnTransmittance * cosine2;
	result1.g += lnTransmittance * sine2;

	const float sine3 = sine2 * cosine + cosine2 * sine;
	const float cosine3 = cosine2 * cosine - sine2 * sine;
	result1.b += lnTransmittance * cosine3;
	result1.a += lnTransmittance * sine3;
}

float fourierOpacityGetLogTransmittance(float depth, float4 fom0, float4 fom1)
{
	const float depthTwoPi = depth * 2.0 * PI;
	
	float lnTransmittance = fom0.r * 0.5 * depth;

	float sine, cosine;
	sincos(depthTwoPi, sine, cosine);
	lnTransmittance += fom0.b / (2.0 * PI * 1.0) * sine;
	lnTransmittance += fom0.a / (2.0 * PI * 1.0) * (1.0 - cosine);

	const float sine2 = sine * cosine + cosine * sine;
	const float cosine2 = cosine * cosine - sine * sine;
	lnTransmittance += fom1.r / (2.0 * PI * 2.0) * sine2;
	lnTransmittance += fom1.g / (2.0 * PI * 2.0) * (1.0 - cosine2);

	const float sine3 = sine2 * cosine + cosine2 * sine;
	const float cosine3 = cosine2 * cosine - sine2 * sine;
	lnTransmittance += fom1.b / (2.0 * PI * 3.0) * sine3;
	lnTransmittance += fom1.a / (2.0 * PI * 3.0) * (1.0 - cosine3);
	
	return lnTransmittance;
}

float fourierOpacityGetTransmittance(float depth, float4 fom0, float4 fom1)
{
	float lnTransmittance = fourierOpacityGetLogTransmittance(depth, fom0, fom1);

	return saturate(exp(-lnTransmittance));
}

#endif // COMMON_FOURIER_OPACITY
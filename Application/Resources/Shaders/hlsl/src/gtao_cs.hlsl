#include "bindingHelper.hlsli"
#include "gtao.hlsli"
#include "common.hlsli"

RWTexture2D<float2> g_ResultImage : REGISTER_UAV(RESULT_IMAGE_BINDING, 0);
Texture2D<float> g_DepthImage : REGISTER_SRV(DEPTH_IMAGE_BINDING, 0);

SamplerState g_Samplers[SAMPLER_COUNT] : REGISTER_SAMPLER(0, 1);

PUSH_CONSTS(PushConsts, g_PushConsts);

float3 getViewSpacePos(float2 uv)
{
	uv *= g_PushConsts.resolution.zw;
	float depth = g_DepthImage.SampleLevel(g_Samplers[SAMPLER_POINT_CLAMP], uv, 0.0).x;
	float2 clipSpacePosition = float2(uv * float2(2.0, -2.0) - float2(1.0, -1.0));
	float4 viewSpacePosition = float4(g_PushConsts.unprojectParams.xy * clipSpacePosition, -1.0, g_PushConsts.unprojectParams.z * depth + g_PushConsts.unprojectParams.w);
	return viewSpacePosition.xyz / viewSpacePosition.w;
}

float3 minDiff(float3 P, float3 Pr, float3 Pl)
{
    float3 V1 = Pr - P;
    float3 V2 = P - Pl;
    return (dot(V1, V1) < dot(V2, V2)) ? V1 : V2;
}

void computeSteps(inout float stepSizePix, inout float numSteps, float rayRadiusPix)
{
    // Avoid oversampling if numSteps is greater than the kernel radius in pixels
    numSteps = min(g_PushConsts.numSteps, rayRadiusPix);
    stepSizePix = rayRadiusPix / numSteps;
}

// x = spatial direction / y = temporal direction / z = spatial offset / w = temporal offset
float4 getNoise(int2 coord)
{
	float4 noise;
	
	noise.x = (1.0 / 16.0) * ((((coord.x + coord.y) & 0x3 ) << 2) + (coord.x & 0x3));
	noise.z = (1.0 / 4.0) * ((coord.y - coord.x) & 0x3);

	const float rotations[] = { 60.0, 300.0, 180.0, 240.0, 120.0, 0.0 };
	noise.y = rotations[g_PushConsts.frame % 6] * (1.0 / 360.0);
	
	const float offsets[] = { 0.0, 0.5, 0.25, 0.75 };
	noise.w = offsets[(g_PushConsts.frame / 6 ) % 4];
	
	return noise;
}

float square(float x)
{
	return x * x;
}

float falloff(float dist2)
{
	float start = square(g_PushConsts.radius * 0.8);
	float end = square(g_PushConsts.radius);
	return 2.0 * saturate((dist2 - start) / (end - start));
}

float fastSqrt(float x)
{
	// [Drobot2014a] Low Level Optimizations for GCN
	return asfloat(0x1FBD1DF5 + (asint(x) >> 1));
}

float fastAcos(float x)
{
	// [Eberly2014] GPGPU Programming for Games and Science
	float res = -0.156583 * abs(x) + PI / 2.0;
	res *= fastSqrt(1.0 - abs(x));
	return x >= 0 ? res : PI - res;
}

float integrateArcCosineWeighted(float h1, float h2, float gamma, float cosGamma)
{
	float sinGamma = sin(gamma);
	
	h1 *= 2.0;
	h2 *= 2.0;
	float a = (-cos(h1 - gamma) + (h1 * sinGamma + cosGamma));
	float b = (-cos(h2 - gamma) + (h2 * sinGamma + cosGamma));
	return (a + b) * 0.25;
}

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	if (threadID.x >= g_PushConsts.resolution.x || threadID.y >= g_PushConsts.resolution.y)
	{
		return;
	}

	float3 P = getViewSpacePos(float2(threadID.xy + 0.5));

	// Calculate the projected size of the hemisphere
    float rayRadiusUV = 0.5 * g_PushConsts.radius * g_PushConsts.focalLength / -P.z;
    float rayRadiusPix = rayRadiusUV * g_PushConsts.resolution.x;
	rayRadiusPix = min(rayRadiusPix, g_PushConsts.maxRadiusPixels);

    float ao = 1.0;

    // Make sure the radius of the evaluated hemisphere is more than a pixel
    if(rayRadiusPix > 1.0)
    {
		ao = 0.0;
		
		const float3 V = -normalize(P);
		
		float numSteps;
    	float stepSizePix;

    	// Compute the number of steps
    	computeSteps(stepSizePix, numSteps, rayRadiusPix);

		float4 noise = getNoise(threadID.xy);

		float theta = (noise.x + noise.y) * PI;
		float jitter = noise.z + noise.w;
		float2 dir;
		sincos(theta, dir.y, dir.x);
		float2 horizons = -1.0;
		
		float currstep = frac(jitter) * (stepSizePix - 1.0) + 1.0;

		for(float s = 0; s < numSteps; ++s)
		{
			float2 offset = currstep * dir;
			currstep += stepSizePix;
			
			// first horizon
			{
				float3 S = getViewSpacePos(float2(threadID.xy + 0.5) + offset);
				float3 D = S - P;
				float dist2 = dot(D, D);
				D *= rsqrt(dist2);
				float attenuation = falloff(dist2);
				horizons.x = max(dot(V, D) - attenuation, horizons.x);
			}
			
			// second horizon
			{
				float3 S = getViewSpacePos(float2(threadID.xy + 0.5) - offset);
				float3 D = S - P;
				float dist2 = dot(D, D);
				D *= rsqrt(dist2);
				float attenuation = falloff(dist2);
				horizons.y = max(dot(V, D) - attenuation, horizons.y);
			}
		}
		
		// Sample neighboring pixels
		const float3 Pr = getViewSpacePos(float2(threadID.xy + 0.5) + float2(1.0, 0.0));
		const float3 Pl = getViewSpacePos(float2(threadID.xy + 0.5) + float2(-1.0, 0.0));
		const float3 Pb = getViewSpacePos(float2(threadID.xy + 0.5) + float2(0.0, 1.0));
		const float3 Pt = getViewSpacePos(float2(threadID.xy + 0.5) + float2(0.0, -1.0));
		
		// Calculate tangent basis vectors using the minimum difference
		const float3 dPdu = minDiff(P, Pr, Pl);
		const float3 dPdv = minDiff(P, Pt, Pb);
		
		const float3 N = normalize(cross(dPdu, dPdv));
		
		// project normal onto slice plane
		// invert dir.y because screen space y axis points down, but view space y axis points up
		const float3 planeN = normalize(cross(float3(dir.x, -dir.y, 0.0), V));
		float3 projectedN = N - dot(N, planeN) * planeN;
		
		float projectedNLength = length(projectedN);
		projectedN = normalize(projectedN);
		
		// calculate gamma
		float3 tangent = cross(V, planeN);
		float cosGamma	= dot(projectedN, V);
		float gamma = fastAcos(cosGamma);
		// reconstruct the sign
		gamma = dot(projectedN, tangent) < 0.0 ? gamma : -gamma;
		
		float h1 = -fastAcos(horizons.x);
		float h2 = fastAcos(horizons.y);
		
		// clamp horizons
		h1 = gamma + max(h1 - gamma, -PI * 0.5);
		h2 = gamma + min(h2 - gamma, PI * 0.5);
		
		ao += projectedNLength * integrateArcCosineWeighted(h1, h2, gamma, cosGamma);
	}
	
	ao = saturate(ao);

	g_ResultImage[threadID.xy] = float2(ao, -P.z);
}
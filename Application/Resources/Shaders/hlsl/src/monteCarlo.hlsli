#ifndef MONTE_CARLO_H
#define MONTE_CARLO_H

#include "brdf.hlsli"

// based on information from "Physically Based Shading at Disney" by Brent Burley
float3 importanceSampleGGX(float2 noise, float3 N, float a2)
{
	float phi = 2.0 * PI * noise.x;
	float cosTheta = sqrt((1.0 - noise.y) / (1.0 + (a2 - 1.0) * noise.y + 1e-5) + 1e-5);
	cosTheta = saturate(cosTheta);
	float sinTheta = sqrt(1.0 - cosTheta * cosTheta + 1e-5);
	
	float sinPhi;
	float cosPhi;
	sincos(phi, sinPhi, cosPhi);
	float3 H = float3(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta);
	
	// transform H from tangent-space to world-space
	float3 up = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
	float3 tangent = normalize(cross(up, N));
	float3 bitangent = cross(N, tangent);
	
	return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

float pdfGGX(float3 N, float3 V, float3 H, float a2)
{
	float NdotH = saturate(dot(N, H));
	float VdotH = saturate(dot(V, H));
	return D_GGX(NdotH, a2) * NdotH / (4.0 * VdotH);
}

#endif // MONTE_CARLO_H
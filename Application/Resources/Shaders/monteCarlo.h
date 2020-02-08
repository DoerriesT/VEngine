#ifndef MONTE_CARLO_H
#define MONTE_CARLO_H

#include "brdf.h"

// based on information from "Physically Based Shading at Disney" by Brent Burley
vec3 importanceSampleGGX(vec2 noise, vec3 N, float a2)
{
	float phi = 2.0 * PI * noise.x;
	float cosTheta = sqrt((1.0 - noise.y) / (1.0 + (a2 - 1.0) * noise.y + 1e-5) + 1e-5);
	cosTheta = clamp(cosTheta, 0.0, 1.0);
	float sinTheta = sqrt(1.0 - cosTheta * cosTheta + 1e-5);
	
	vec3 H = vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
	
	// transform H from tangent-space to world-space
	vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
	vec3 tangent = normalize(cross(up, N));
	vec3 bitangent = cross(N, tangent);
	
	return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

float pdfGGX(vec3 N, vec3 V, vec3 H, float a2)
{
	float NdotH = max(dot(N, H), 0.0);
	float VdotH = max(dot(V, H), 0.0);
	return D_GGX(NdotH, a2) * NdotH / (4.0 * VdotH);
}

#endif // MONTE_CARLO_H
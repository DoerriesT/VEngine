#ifndef SHADING_MODEL_H
#define SHADING_MODEL_H

#include "brdf.h"

vec3 Default_Lit(vec3 baseColor, vec3 F0, vec3 radiance, vec3 N, vec3 V, vec3 L, float roughness, float metalness)
{
	// avoid precision problems
	roughness = max(roughness, 0.04);
	float NdotV = abs(dot(N, V)) + 1e-5;
	vec3 H = normalize(V + L);
	float VdotH = max(dot(V, H), 0.0);
	float NdotH = max(dot(N, H), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	
	float a = roughness * roughness;
	float a2 = a * a;
	
	vec3 specular = Specular_GGX(F0, NdotV, NdotL, NdotH, VdotH, a2);
	
	// TODO: how can the diffuse term be made energy conserving with respect to the specular term?
	//vec3 diffuse = Diffuse_Lambert(baseColor);// * (1.0 - F_Schlick(F0, NdotL)) * (1.0 - F_Schlick(F0, NdotV));
	//vec3 diffuse = (21.0 / (20.0 * PI)) * (1.0 - F0) * baseColor * (1.0 - pow5(1.0 - NdotL)) * (1.0 - pow5(1.0 - NdotV));
	vec3 diffuse = Diffuse_Disney(NdotV, NdotL, VdotH, roughness, baseColor);
	
	diffuse *= 1.0 - metalness;
	
	return (specular + diffuse) * radiance * NdotL;
}

vec3 Diffuse_Lit(vec3 baseColor, vec3 radiance, vec3 N, vec3 L)
{
	vec3 diffuse = Diffuse_Lambert(baseColor);
	float NdotL = max(dot(N, L), 0.0);
	return diffuse * radiance * NdotL;
}

#endif // SHADING_MODEL_H
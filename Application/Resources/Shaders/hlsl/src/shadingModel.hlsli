#ifndef SHADING_MODEL_H
#define SHADING_MODEL_H

#include "brdf.hlsli"

float3 Default_Lit(float3 baseColor, float3 F0, float3 radiance, float3 N, float3 V, float3 L, float roughness, float metalness)
{
	// avoid precision problems
	roughness = max(roughness, 0.04);
	float NdotV = abs(dot(N, V)) + 1e-5;
	float3 H = normalize(V + L);
	float VdotH = saturate(dot(V, H));
	float NdotH = saturate(dot(N, H));
	float NdotL = saturate(dot(N, L));
	
	float a = roughness * roughness;
	float a2 = a * a;
	
	float3 specular = Specular_GGX(F0, NdotV, NdotL, NdotH, VdotH, a2);
	
	// TODO: how can the diffuse term be made energy conserving with respect to the specular term?
	//float3 diffuse = Diffuse_Lambert(baseColor);// * (1.0 - F_Schlick(F0, NdotL)) * (1.0 - F_Schlick(F0, NdotV));
	//float3 diffuse = (21.0 / (20.0 * PI)) * (1.0 - F0) * baseColor * (1.0 - pow5(1.0 - NdotL)) * (1.0 - pow5(1.0 - NdotV));
	float3 diffuse = Diffuse_Disney(NdotV, NdotL, VdotH, roughness, baseColor);
	
	diffuse *= 1.0 - metalness;
	
	return (specular + diffuse) * radiance * NdotL;
}

float3 Diffuse_Lit(float3 baseColor, float3 radiance, float3 N, float3 L)
{
	float3 diffuse = Diffuse_Lambert(baseColor);
	float NdotL = saturate(dot(N, L));
	return diffuse * radiance * NdotL;
}

#endif // SHADING_MODEL_H
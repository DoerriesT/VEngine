#version 450

#include "common.h"

#ifndef ALPHA_MASK_ENABLED
#define ALPHA_MASK_ENABLED 0
#endif // ALPHA_MASK_ENABLED

#ifndef ALPHA_CUTOFF
#define ALPHA_CUTOFF (0.9)
#endif // ALPHA_CUTOFF

#ifndef MIP_SCALE
#define MIP_SCALE (0.25)
#endif // MIP_SCALE

layout(push_constant) uniform PushConsts 
{
	uint transformIndex;
	uint materialIndex;
} uPushConsts;

#if !ALPHA_MASK_ENABLED
layout(early_fragment_tests) in;
#endif // ALPHA_MASK_ENABLED

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec3 vWorldPos;

layout(location = 0) out vec4 oAlbedo;
layout(location = 1) out vec4 oNormalEmissive;
layout(location = 2) out vec4 oMetalnessRoughnessOcclusion;
//layout(location = 3) out vec4 oVelocity;

// based on http://www.thetenthplanet.de/archives/1180
mat3 calculateTBN( vec3 N, vec3 p, vec2 uv )
{
    // get edge vectors of the pixel triangle
    vec3 dp1 = dFdx( p );
    vec3 dp2 = dFdy( p );
    vec2 duv1 = dFdx( uv );
    vec2 duv2 = dFdy( uv );
 
    // solve the linear system
    vec3 dp2perp = cross( dp2, N );
    vec3 dp1perp = cross( N, dp1 );
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;
 
    // construct a scale-invariant frame 
    float invmax = inversesqrt( max( dot(T,T), dot(B,B) ) );
    return mat3( T * -invmax, B * -invmax, N );
}

void main() 
{
	MaterialData materialData = uMaterialData.data[uPushConsts.materialIndex - 1];
	
	// albedo
	{
		vec3 albedo = materialData.albedoMetalness.rgb;
		uint albedoTextureIndex = (materialData.albedoNormalTexture & 0xFFFF0000) >> 16;
		if (albedoTextureIndex != 0)
		{
#if ALPHA_MASK_ENABLED
			vec4 albedoTexSample = texture(uTextures[albedoTextureIndex - 1], vTexCoord).rgba;
			albedoTexSample.a *= 1.0 + textureQueryLod(uTextures[albedoTextureIndex - 1], vTexCoord).x * MIP_SCALE;
			if(albedoTexSample.a < ALPHA_CUTOFF)
			{
				discard;
			}
#else
			vec3 albedoTexSample = texture(uTextures[albedoTextureIndex - 1], vTexCoord).rgb;
#endif // ALPHA_MASK_ENABLED
			albedo *= albedoTexSample.rgb;
		}
		oAlbedo.rgb = albedo;
	}
	
	// normal
	{
		vec3 normal = normalize(vNormal);
		uint normalTextureIndex = (materialData.albedoNormalTexture & 0xFFFF);
		if (normalTextureIndex != 0)
		{
			mat3 tbn = calculateTBN(normal, vWorldPos, vTexCoord);
			normal = normalize(tbn * (texture(uTextures[normalTextureIndex - 1], vTexCoord).xyz * 2.0 - 1.0));
		}
		oNormalEmissive.xyz = normal;
	}
	
	vec3 metalnessRoughnessOcclusion = vec3(materialData.albedoMetalness.w, materialData.emissiveRoughness.w, 1.0);
	
	// metalness
	{
		uint metalnessTextureIndex = (materialData.metalnessRoughnessTexture & 0xFFFF0000) >> 16;
		if (metalnessTextureIndex != 0)
		{
			metalnessRoughnessOcclusion.x *= texture(uTextures[metalnessTextureIndex - 1], vTexCoord).x;
		}
	}
	
	// roughness
	{
		uint roughnessTextureIndex = (materialData.metalnessRoughnessTexture & 0xFFFF);
		if (roughnessTextureIndex != 0)
		{
			metalnessRoughnessOcclusion.y *= texture(uTextures[roughnessTextureIndex - 1], vTexCoord).x;
		}
	}
	
	// occlusion
	{
		uint occlusionTextureIndex = (materialData.occlusionEmissiveTexture & 0xFFFF0000) >> 16;
		if (occlusionTextureIndex != 0)
		{
			metalnessRoughnessOcclusion.z *= texture(uTextures[occlusionTextureIndex - 1], vTexCoord).x;
		}
	}
	
	oMetalnessRoughnessOcclusion.xyz = metalnessRoughnessOcclusion;
}


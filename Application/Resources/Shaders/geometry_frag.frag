#version 450

#include "geometry_bindings.h"

layout(set = MATERIAL_DATA_SET, binding = MATERIAL_DATA_BINDING) readonly buffer MATERIAL_DATA 
{
    MaterialData uMaterialData[];
};

layout(set = TEXTURES_SET, binding = TEXTURES_BINDING) uniform sampler2D uTextures[TEXTURE_ARRAY_SIZE];

#ifndef ALPHA_MASK_ENABLED
#define ALPHA_MASK_ENABLED 0
#endif // ALPHA_MASK_ENABLED

#if !ALPHA_MASK_ENABLED
layout(early_fragment_tests) in;
#endif // ALPHA_MASK_ENABLED

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec3 vWorldPos;
layout(location = 3) flat in uint vMaterialIndex;

layout(location = OUT_ALBEDO) out vec4 oAlbedo;
layout(location = OUT_NORMAL) out vec4 oNormalEmissive;
layout(location = OUT_METALNESS_ROUGHNESS_OCCLUSION) out vec4 oMetalnessRoughnessOcclusion;
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
	MaterialData materialData = uMaterialData[vMaterialIndex];
	
	// albedo
	{
		vec3 albedo = unpackUnorm4x8(materialData.albedoOpacity).rgb;
		uint albedoTextureIndex = (materialData.albedoNormalTexture & 0xFFFF0000) >> 16;
		if (albedoTextureIndex != 0)
		{
#if ALPHA_MASK_ENABLED
			vec4 albedoTexSample = texture(uTextures[albedoTextureIndex - 1], vTexCoord).rgba;
			albedoTexSample.a *= 1.0 + textureQueryLod(uTextures[albedoTextureIndex - 1], vTexCoord).x * ALPHA_MIP_SCALE;
			if(albedoTexSample.a < ALPHA_CUTOFF)
			{
				discard;
			}
#else
			vec3 albedoTexSample = texture(uTextures[albedoTextureIndex - 1], vTexCoord).rgb;
#endif // ALPHA_MASK_ENABLED
			albedo = albedoTexSample.rgb;
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
			vec3 tangentSpaceNormal;
			tangentSpaceNormal.xy = texture(uTextures[normalTextureIndex - 1], vTexCoord).xy * 2.0 - 1.0;
			tangentSpaceNormal.z = sqrt(1 - tangentSpaceNormal.x * tangentSpaceNormal.x + tangentSpaceNormal.y * tangentSpaceNormal.y);
			normal = normalize(tbn * tangentSpaceNormal);
		}
		oNormalEmissive.xyz = normal;
	}
	
	vec3 metalnessRoughnessOcclusion = vec3(unpackUnorm4x8(materialData.metalnessRoughness).xy, 1.0);
	
	// metalness
	{
		uint metalnessTextureIndex = (materialData.metalnessRoughnessTexture & 0xFFFF0000) >> 16;
		if (metalnessTextureIndex != 0)
		{
			metalnessRoughnessOcclusion.x = texture(uTextures[metalnessTextureIndex - 1], vTexCoord).z;
		}
	}
	
	// roughness
	{
		uint roughnessTextureIndex = (materialData.metalnessRoughnessTexture & 0xFFFF);
		if (roughnessTextureIndex != 0)
		{
			metalnessRoughnessOcclusion.y = texture(uTextures[roughnessTextureIndex - 1], vTexCoord).y;
		}
	}
	
	// occlusion
	{
		uint occlusionTextureIndex = (materialData.occlusionEmissiveTexture & 0xFFFF0000) >> 16;
		if (occlusionTextureIndex != 0)
		{
			metalnessRoughnessOcclusion.z = texture(uTextures[occlusionTextureIndex - 1], vTexCoord).x;
		}
	}
	
	oMetalnessRoughnessOcclusion.xyz = metalnessRoughnessOcclusion;
}


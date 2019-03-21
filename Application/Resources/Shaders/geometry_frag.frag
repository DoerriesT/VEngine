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
	uint drawIndex;
} uPushConsts;

#if !ALPHA_MASK_ENABLED
layout(early_fragment_tests) in;
#endif // ALPHA_MASK_ENABLED

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec3 vWorldPos;

layout(location = 0) out vec4 oAlbedo;
layout(location = 1) out vec4 oNormalEmissive;
layout(location = 2) out vec4 oMetallicRoughnessOcclusion;
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
#if ALPHA_MASK_ENABLED
	vec3 albedo = uPerDrawData.data[uPushConsts.drawIndex].albedoFactorMetallic.rgb;
	if (uPerDrawData.data[uPushConsts.drawIndex].albedoTexture != 0)
	{
		vec4 albedoTexSample = texture(uTextures[uPerDrawData.data[uPushConsts.drawIndex].albedoTexture - 1], vTexCoord).rgba;
		albedoTexSample.a *= 1.0 + textureQueryLod(uTextures[uPerDrawData.data[uPushConsts.drawIndex].albedoTexture - 1], vTexCoord).x * MIP_SCALE;
		if(albedoTexSample.a < ALPHA_CUTOFF)
		{
			discard;
		}
		albedo *= albedoTexSample.rgb;
	}
	oAlbedo.rgb = albedo;
#else
	oAlbedo.rgb = uPerDrawData.data[uPushConsts.drawIndex].albedoFactorMetallic.rgb;
#endif // ALPHA_MASK_ENABLED
	
	oNormalEmissive.xyz = normalize(vNormal);
	oMetallicRoughnessOcclusion.x = uPerDrawData.data[uPushConsts.drawIndex].albedoFactorMetallic.w;
	oMetallicRoughnessOcclusion.y = uPerDrawData.data[uPushConsts.drawIndex].emissiveFactorRoughness.w;
	oMetallicRoughnessOcclusion.z = 1.0;
	
#if !ALPHA_MASK_ENABLED
	oAlbedo.rgb *= (uPerDrawData.data[uPushConsts.drawIndex].albedoTexture != 0) ? texture(uTextures[uPerDrawData.data[uPushConsts.drawIndex].albedoTexture - 1], vTexCoord).rgb : vec3(1.0);
#endif // !ALPHA_MASK_ENABLED
	
	oNormalEmissive.xyz = (uPerDrawData.data[uPushConsts.drawIndex].normalTexture != 0) ? 
	normalize(calculateTBN(oNormalEmissive.xyz, vWorldPos, vTexCoord) * (texture(uTextures[uPerDrawData.data[uPushConsts.drawIndex].normalTexture - 1], vTexCoord).xyz * 2.0 - 1.0)) : oNormalEmissive.xyz;
	
	oMetallicRoughnessOcclusion.x *= (uPerDrawData.data[uPushConsts.drawIndex].metallicTexture != 0) ? texture(uTextures[uPerDrawData.data[uPushConsts.drawIndex].metallicTexture - 1], vTexCoord).x : 1.0;
	oMetallicRoughnessOcclusion.y *= (uPerDrawData.data[uPushConsts.drawIndex].roughnessTexture != 0) ? texture(uTextures[uPerDrawData.data[uPushConsts.drawIndex].roughnessTexture - 1], vTexCoord).x : 1.0;
	oMetallicRoughnessOcclusion.z *= (uPerDrawData.data[uPushConsts.drawIndex].occlusionTexture != 0) ? texture(uTextures[uPerDrawData.data[uPushConsts.drawIndex].occlusionTexture - 1], vTexCoord).x : 1.0;
}


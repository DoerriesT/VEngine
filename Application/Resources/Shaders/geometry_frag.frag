#version 450
#extension GL_ARB_separate_shader_objects : enable

#ifndef PI
#define PI (3.14159265359)
#endif // PI

#ifndef TEXTURE_ARRAY_SIZE
#define TEXTURE_ARRAY_SIZE (512)
#endif // TEXTURE_ARRAY_SIZE

#ifndef ALPHA_MASK_ENABLED
#define ALPHA_MASK_ENABLED 0
#endif // ALPHA_MASK_ENABLED

#ifndef ALPHA_CUTOFF
#define ALPHA_CUTOFF (0.9)
#endif // ALPHA_CUTOFF

#ifndef MIP_SCALE
#define MIP_SCALE (0.25)
#endif // MIP_SCALE

#if !ALPHA_MASK_ENABLED
layout(early_fragment_tests) in;
#endif // ALPHA_MASK_ENABLED

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec3 vWorldPos;

layout(location = 0) out vec4 oAlbedo;
layout(location = 1) out vec4 oNormalEmissive;
layout(location = 2) out vec4 oMetallicRoughnessOcclusion;
layout(location = 3) out vec4 oVelocity;

layout(set = 0, binding = 0) uniform PerFrameData 
{
	float time;
	float fovy;
	float nearPlane;
	float farPlane;
	mat4 viewMatrix;
	mat4 projectionMatrix;
	mat4 viewProjectionMatrix;
	mat4 invViewMatrix;
	mat4 invProjectionMatrix;
	mat4 invViewProjectionMatrix;
	mat4 prevViewMatrix;
	mat4 prevProjectionMatrix;
	mat4 prevViewProjectionMatrix;
	mat4 prevInvViewMatrix;
	mat4 prevInvProjectionMatrix;
	mat4 prevInvViewProjectionMatrix;
	vec4 cameraPosition;
	vec4 cameraDirection;
	uint frame;
} uPerFrameData;

layout(set = 1, binding = 0) uniform PerDrawData 
{
    vec4 albedoFactorMetallic;
	vec4 emissiveFactorRoughness;
	mat4 modelMatrix;
	uint albedoTexture;
	uint normalTexture;
	uint metallicTexture;
	uint roughnessTexture;
	uint occlusionTexture;
	uint emissiveTexture;
	uint displacementTexture;
	uint padding;
} uPerDrawData;

layout(set = 2, binding = 0) uniform sampler2D uTextures[TEXTURE_ARRAY_SIZE];

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
	vec3 albedo = uPerDrawData.albedoFactorMetallic.rgb;
	if (uPerDrawData.albedoTexture != 0)
	{
		vec4 albedoTexSample = texture(uTextures[uPerDrawData.albedoTexture], vTexCoord).rgba;
		albedoTexSample.a *= 1.0 + textureQueryLod(uTextures[uPerDrawData.albedoTexture], vTexCoord).x * MIP_SCALE;
		if(albedoTexSample.a < ALPHA_CUTOFF)
		{
			discard;
		}
		albedo *= albedoTexSample.rgb;
	}
	oAlbedo.rgb = albedo;
#else
	oAlbedo.rgb = uPerDrawData.albedoFactorMetallic.rgb;
#endif // ALPHA_MASK_ENABLED
	
	oNormalEmissive.xyz = normalize(vNormal);
	oMetallicRoughnessOcclusion.x = uPerDrawData.albedoFactorMetallic.w;
	oMetallicRoughnessOcclusion.y = uPerDrawData.emissiveFactorRoughness.w;
	oMetallicRoughnessOcclusion.z = 1.0;
	
#if !ALPHA_MASK_ENABLED
	oAlbedo.rgb *= (uPerDrawData.albedoTexture != 0) ? texture(uTextures[uPerDrawData.albedoTexture], vTexCoord).rgb : vec3(1.0);
#endif // !ALPHA_MASK_ENABLED
	
	oNormalEmissive.xyz = (uPerDrawData.normalTexture != 0) ? 
	normalize(calculateTBN(oNormalEmissive.xyz, vWorldPos, vTexCoord) * (texture(uTextures[uPerDrawData.normalTexture], vTexCoord).xyz * 2.0 - 1.0)) : oNormalEmissive.xyz;
	
	oMetallicRoughnessOcclusion.x *= (uPerDrawData.metallicTexture != 0) ? texture(uTextures[uPerDrawData.metallicTexture], vTexCoord).x : 1.0;
	oMetallicRoughnessOcclusion.y *= (uPerDrawData.roughnessTexture != 0) ? texture(uTextures[uPerDrawData.roughnessTexture], vTexCoord).x : 1.0;
	oMetallicRoughnessOcclusion.z *= (uPerDrawData.occlusionTexture != 0) ? texture(uTextures[uPerDrawData.occlusionTexture], vTexCoord).x : 1.0;
}


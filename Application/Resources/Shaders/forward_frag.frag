#version 450

#extension GL_KHR_shader_subgroup_ballot : enable
#extension GL_KHR_shader_subgroup_arithmetic : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "forward_bindings.h"
#include "common.h"
#include "commonLighting.h"
#include "srgb.h"

#define VOLUME_DEPTH (64)
#define VOLUME_NEAR (0.5)
#define VOLUME_FAR (64.0)

layout(constant_id = DIRECTIONAL_LIGHT_COUNT_CONST_ID) const uint cDirectionalLightCount = 1;
layout(constant_id = WIDTH_CONST_ID) const uint cWidth = 1600;
layout(constant_id = HEIGHT_CONST_ID) const uint cHeight = 900;
layout(constant_id = TEXEL_WIDTH_CONST_ID) const float cTexelWidth = 1.0 / 1600.0;
layout(constant_id = TEXEL_HEIGHT_CONST_ID) const float cTexelHeight = 1.0 / 900;

layout(set = SSAO_IMAGE_SET, binding = SSAO_IMAGE_BINDING) uniform texture2D uAmbientOcclusionImage;
layout(set = DEFERRED_SHADOW_IMAGE_SET, binding = DEFERRED_SHADOW_IMAGE_BINDING) uniform texture2DArray uDeferredShadowImage;
layout(set = VOLUMETRIC_FOG_IMAGE_SET, binding = VOLUMETRIC_FOG_IMAGE_BINDING) uniform texture3D uVolumetricFogImage;
layout(set = POINT_SAMPLER_SET, binding = POINT_SAMPLER_BINDING) uniform sampler uPointSampler;

layout(set = DIRECTIONAL_LIGHT_DATA_SET, binding = DIRECTIONAL_LIGHT_DATA_BINDING) readonly buffer DIRECTIONAL_LIGHT_DATA
{
	DirectionalLightData uDirectionalLightData[];
};

layout(set = POINT_LIGHT_DATA_SET, binding = POINT_LIGHT_DATA_BINDING) readonly buffer POINT_LIGHT_DATA
{
	PointLightData uPointLightData[];
};

layout(set = POINT_LIGHT_Z_BINS_SET, binding = POINT_LIGHT_Z_BINS_BINDING) readonly buffer POINT_LIGHT_Z_BINS
{
	uint uPointLightZBins[];
};

layout(set = POINT_LIGHT_MASK_SET, binding = POINT_LIGHT_MASK_BINDING) readonly buffer POINT_LIGHT_BITMASK 
{
	uint uPointLightBitMask[];
};

layout(set = SPOT_LIGHT_DATA_SET, binding = SPOT_LIGHT_DATA_BINDING) readonly buffer SPOT_LIGHT_DATA
{
	SpotLightData uSpotLightData[];
};

layout(set = SPOT_LIGHT_Z_BINS_SET, binding = SPOT_LIGHT_Z_BINS_BINDING) readonly buffer SPOT_LIGHT_Z_BINS
{
	uint uSpotLightZBins[];
};

layout(set = SPOT_LIGHT_MASK_SET, binding = SPOT_LIGHT_MASK_BINDING) readonly buffer SPOT_LIGHT_BITMASK 
{
	uint uSpotLightBitMask[];
};

layout(set = MATERIAL_DATA_SET, binding = MATERIAL_DATA_BINDING) readonly buffer MATERIAL_DATA 
{
    MaterialData uMaterialData[];
};

layout(set = TEXTURES_SET, binding = TEXTURES_BINDING) uniform texture2D uTextures[TEXTURE_ARRAY_SIZE];
layout(set = SAMPLERS_SET, binding = SAMPLERS_BINDING) uniform sampler uSamplers[SAMPLER_COUNT];

layout(push_constant) uniform PUSH_CONSTS 
{
	PushConsts uPushConsts;
};

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec4 vWorldPos;
layout(location = 3) flat in uint vMaterialIndex;

layout(location = OUT_RESULT) out vec4 oResult;
layout(location = OUT_NORMAL) out vec4 oNormal;
layout(location = OUT_SPECULAR_ROUGHNESS) out vec4 oSpecularRoughness;

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
    return mat3( T * invmax, B * invmax, N );
}

void main() 
{
	const MaterialData materialData = uMaterialData[vMaterialIndex];
	
	LightingParams lightingParams;
	lightingParams.viewSpacePosition = vWorldPos.xyz / vWorldPos.w;
	lightingParams.V = -normalize(lightingParams.viewSpacePosition);
	vec4 derivatives = vec4(dFdx(vTexCoord), dFdy(vTexCoord));
	
	// albedo
	{
		vec3 albedo = unpackUnorm4x8(materialData.albedoOpacity).rgb;
		uint albedoTextureIndex = (materialData.albedoNormalTexture & 0xFFFF0000) >> 16;
		if (albedoTextureIndex != 0)
		{
			vec4 albedoTexSample = textureGrad(sampler2D(uTextures[nonuniformEXT(albedoTextureIndex - 1)], uSamplers[SAMPLER_LINEAR_REPEAT]), vTexCoord, derivatives.xy, derivatives.zw).rgba;
			albedo = albedoTexSample.rgb;
		}
		lightingParams.albedo = accurateSRGBToLinear(albedo);
	}
	
	// normal
	{
		vec3 normal = normalize(vNormal);
		uint normalTextureIndex = (materialData.albedoNormalTexture & 0xFFFF);
		if (normalTextureIndex != 0)
		{
			vec3 tangentSpaceNormal;
			tangentSpaceNormal.xy = textureGrad(sampler2D(uTextures[nonuniformEXT(normalTextureIndex - 1)], uSamplers[SAMPLER_LINEAR_REPEAT]), vTexCoord, derivatives.xy, derivatives.zw).xy * 2.0 - 1.0;
			tangentSpaceNormal.z = sqrt(1 - tangentSpaceNormal.x * tangentSpaceNormal.x + tangentSpaceNormal.y * tangentSpaceNormal.y);
			normal = calculateTBN(normal, lightingParams.viewSpacePosition, vec2(vTexCoord.x, -vTexCoord.y)) * tangentSpaceNormal;
			normal = normalize(normal);
		}
		lightingParams.N = normal;
	}
	
	// metalness
	{
		float metalness = unpackUnorm4x8(materialData.metalnessRoughness).x;
		uint metalnessTextureIndex = (materialData.metalnessRoughnessTexture & 0xFFFF0000) >> 16;
		if (metalnessTextureIndex != 0)
		{
			metalness = textureGrad(sampler2D(uTextures[nonuniformEXT(metalnessTextureIndex - 1)], uSamplers[SAMPLER_LINEAR_REPEAT]), vTexCoord, derivatives.xy, derivatives.zw).z;
		}
		lightingParams.metalness = metalness;
	}
	
	// roughness
	{
		float roughness = unpackUnorm4x8(materialData.metalnessRoughness).y;
		uint roughnessTextureIndex = (materialData.metalnessRoughnessTexture & 0xFFFF);
		if (roughnessTextureIndex != 0)
		{
			roughness = textureGrad(sampler2D(uTextures[nonuniformEXT(roughnessTextureIndex - 1)], uSamplers[SAMPLER_LINEAR_REPEAT]), vTexCoord, derivatives.xy, derivatives.zw).y;
		}
		lightingParams.roughness = roughness;
	}
	
	
	vec3 result = vec3(0.0);
	float ao = 1.0;
	if (uPushConsts.ambientOcclusion != 0)
	{
		ao = texelFetch(sampler2D(uAmbientOcclusionImage, uPointSampler), ivec2(gl_FragCoord.xy), 0).x;
	}
	result = lightingParams.albedo * ao;
	
	// directional lights
	for (uint i = 0; i < cDirectionalLightCount; ++i)
	{
		const DirectionalLightData directionalLightData = uDirectionalLightData[i];
		const vec3 contribution = evaluateDirectionalLight(lightingParams, directionalLightData);
		// TODO: dont assume that every directional light has a shadow mask
		result += contribution * (1.0 - texelFetch(sampler2DArray(uDeferredShadowImage, uPointSampler), ivec3(gl_FragCoord.xy, i), 0).x);
	}
	
	// point lights
	uint pointLightCount = uPushConsts.pointLightCount & 0xFFFF;
	if (pointLightCount > 0)
	{
		uint wordMin = 0;
		const uint wordCount = (pointLightCount + 31) / 32;
		uint wordMax = wordCount - 1;
		
		const uint zBinAddress = clamp(uint(floor(-lightingParams.viewSpacePosition.z)), 0, 8191);
		const uint zBinData = uPointLightZBins[zBinAddress];
		const uint minIndex = (zBinData & uint(0xFFFF0000)) >> 16;
		const uint maxIndex = zBinData & uint(0xFFFF);
		// mergedMin scalar from this point
		const uint mergedMin = subgroupBroadcastFirst(subgroupMin(minIndex)); 
		// mergedMax scalar from this point
		const uint mergedMax = subgroupBroadcastFirst(subgroupMax(maxIndex)); 
		wordMin = max(mergedMin / 32, wordMin);
		wordMax = min(mergedMax / 32, wordMax);
		const uint address = getTileAddress(uvec2(gl_FragCoord.xy), cWidth, wordCount);
		
		for (uint wordIndex = wordMin; wordIndex <= wordMax; ++wordIndex)
		{
			uint mask = uPointLightBitMask[address + wordIndex];
			
			// mask by zbin mask
			const int localBaseIndex = int(wordIndex * 32);
			const uint localMin = clamp(int(minIndex) - localBaseIndex, 0, 31);
			const uint localMax = clamp(int(maxIndex) - localBaseIndex, 0, 31);
			const uint maskWidth = localMax - localMin + 1;
			const uint zBinMask = (maskWidth == 32) ? uint(0xFFFFFFFF) : (((1 << maskWidth) - 1) << localMin);
			mask &= zBinMask;
			
			// compact word bitmask over all threads in subrgroup
			uint mergedMask = subgroupBroadcastFirst(subgroupOr(mask));
			
			while (mergedMask != 0)
			{
				const uint bitIndex = findLSB(mergedMask);
				const uint index = 32 * wordIndex + bitIndex;
				mergedMask ^= (1 << bitIndex);
				result += evaluatePointLight(lightingParams, uPointLightData[index]);
			}
		}
	}
	
	// spot lights
	uint spotLightCount = (uPushConsts.pointLightCount & 0xFFFF0000) >> 16;
	if (spotLightCount > 0)
	{
		uint wordMin = 0;
		const uint wordCount = (spotLightCount + 31) / 32;
		uint wordMax = wordCount - 1;
		
		const uint zBinAddress = clamp(uint(floor(-lightingParams.viewSpacePosition.z)), 0, 8191);
		const uint zBinData = uSpotLightZBins[zBinAddress];
		const uint minIndex = (zBinData & uint(0xFFFF0000)) >> 16;
		const uint maxIndex = zBinData & uint(0xFFFF);
		// mergedMin scalar from this point
		const uint mergedMin = subgroupBroadcastFirst(subgroupMin(minIndex)); 
		// mergedMax scalar from this point
		const uint mergedMax = subgroupBroadcastFirst(subgroupMax(maxIndex)); 
		wordMin = max(mergedMin / 32, wordMin);
		wordMax = min(mergedMax / 32, wordMax);
		const uint address = getTileAddress(uvec2(gl_FragCoord.xy), cWidth, wordCount);
		
		for (uint wordIndex = wordMin; wordIndex <= wordMax; ++wordIndex)
		{
			uint mask = uSpotLightBitMask[address + wordIndex];
			
			// mask by zbin mask
			const int localBaseIndex = int(wordIndex * 32);
			const uint localMin = clamp(int(minIndex) - localBaseIndex, 0, 31);
			const uint localMax = clamp(int(maxIndex) - localBaseIndex, 0, 31);
			const uint maskWidth = localMax - localMin + 1;
			const uint zBinMask = (maskWidth == 32) ? uint(0xFFFFFFFF) : (((1 << maskWidth) - 1) << localMin);
			mask &= zBinMask;
			
			// compact word bitmask over all threads in subrgroup
			uint mergedMask = subgroupBroadcastFirst(subgroupOr(mask));
			
			while (mergedMask != 0)
			{
				const uint bitIndex = findLSB(mergedMask);
				const uint index = 32 * wordIndex + bitIndex;
				mergedMask ^= (1 << bitIndex);
				result += evaluateSpotLight(lightingParams, uSpotLightData[index]);
			}
		}
	}
	
	// apply volumetric fog
	{
		float z = -lightingParams.viewSpacePosition.z;
		float d = (log2(max(0, z * (1.0 / VOLUME_NEAR))) * (1.0 / log2(VOLUME_FAR / VOLUME_NEAR)));
		
		// the fog image can extend further to the right/downwards than the lighting image, so we cant just use the uv
		// of the current texel but instead need to scale the uv with respect to the fog image resolution
		vec2 fogImageSize = vec2(textureSize(sampler3D(uVolumetricFogImage, uSamplers[SAMPLER_LINEAR_CLAMP]), 0).xy);
		vec2 scaledFogImageTexelSize = 1.0 / (fogImageSize * 8.0);
		
		vec3 volumetricFogTexCoord = vec3(gl_FragCoord.xy * scaledFogImageTexelSize, d);
		vec4 fog = textureLod(sampler3D(uVolumetricFogImage, uSamplers[SAMPLER_LINEAR_CLAMP]), volumetricFogTexCoord, 0.0);
		result = result * fog.aaa + fog.rgb;
	}

	oResult = vec4(any(isinf(result)) || any(isnan(result)) ? vec3(1.0, 0.0, 0.0) : result, 1.0);
	oNormal = vec4(lightingParams.N, 1.0);
	oSpecularRoughness = accurateLinearToSRGB(vec4(mix(vec3(0.04), lightingParams.albedo, lightingParams.metalness), lightingParams.roughness));
}
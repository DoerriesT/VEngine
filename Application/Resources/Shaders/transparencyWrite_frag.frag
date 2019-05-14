#version 450

#extension GL_KHR_shader_subgroup_ballot : enable
#extension GL_KHR_shader_subgroup_arithmetic : enable

#include "transparencyWrite_bindings.h"

layout(set = MATERIAL_DATA_SET, binding = MATERIAL_DATA_BINDING) readonly buffer MATERIAL_DATA 
{
    MaterialData uMaterialData[];
};

layout(set = TEXTURES_SET, binding = TEXTURES_BINDING) uniform sampler2D uTextures[TEXTURE_ARRAY_SIZE];

layout(set = SHADOW_ATLAS_SET, binding = SHADOW_ATLAS_BINDING) uniform sampler2DShadow uShadowTexture;

layout(set = DIRECTIONAL_LIGHT_DATA_SET, binding = DIRECTIONAL_LIGHT_DATA_BINDING) readonly buffer DIRECTIONAL_LIGHT_DATA
{
	DirectionalLightData uDirectionalLightData[];
};

layout(set = POINT_LIGHT_DATA_SET, binding = POINT_LIGHT_DATA_BINDING) readonly buffer POINT_LIGHT_DATA
{
	PointLightData uPointLightData[];
};

layout(set = SHADOW_DATA_SET, binding = SHADOW_DATA_BINDING) readonly buffer SHADOW_DATA
{
	ShadowData uShadowData[];
};

layout(set = POINT_LIGHT_Z_BINS_SET, binding = POINT_LIGHT_Z_BINS_BINDING) readonly buffer POINT_LIGHT_Z_BINS
{
	uint uPointLightZBins[];
};

layout(set = POINT_LIGHT_MASK_SET, binding = POINT_LIGHT_MASK_BINDING) readonly buffer POINT_LIGHT_BITMASK 
{
	uint uPointLightBitMask[];
};

layout(push_constant) uniform PUSH_CONSTS 
{
	PushConsts uPushConsts;
};

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec3 vWorldPos;
layout(location = 3) flat in uint vMaterialIndex;

layout(location = OUT_RESULT) out vec4 oResult;

#include "commonLighting.h"

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
	
	LightingParams lightingParams;
	lightingParams.V = -normalize(vWorldPos.xyz);
	lightingParams.viewSpacePosition = vWorldPos.xyz;
	
	// albedo
	{
		vec3 albedo = unpackUnorm4x8(materialData.albedoOpacity).rgb;
		uint albedoTextureIndex = (materialData.albedoNormalTexture & 0xFFFF0000) >> 16;
		if (albedoTextureIndex != 0)
		{
			vec4 albedoTexSample = texture(uTextures[albedoTextureIndex - 1], vTexCoord).rgba;
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
			mat3 tbn = calculateTBN(normal, vWorldPos, vTexCoord);
			vec3 tangentSpaceNormal;
			tangentSpaceNormal.xy = texture(uTextures[normalTextureIndex - 1], vTexCoord).xy * 2.0 - 1.0;
			tangentSpaceNormal.z = sqrt(1 - tangentSpaceNormal.x * tangentSpaceNormal.x + tangentSpaceNormal.y * tangentSpaceNormal.y);
			normal = normalize(tbn * tangentSpaceNormal);
		}
		lightingParams.N =  normal;
	}
	
	// metalness
	{
		float metalness = unpackUnorm4x8(materialData.metalnessRoughness).x;
		uint metalnessTextureIndex = (materialData.metalnessRoughnessTexture & 0xFFFF0000) >> 16;
		if (metalnessTextureIndex != 0)
		{
			metalness = texture(uTextures[metalnessTextureIndex - 1], vTexCoord).z;
		}
		lightingParams.metalness = metalness;
	}
	
	// roughness
	{
		float roughness = unpackUnorm4x8(materialData.metalnessRoughness).y;
		uint roughnessTextureIndex = (materialData.metalnessRoughnessTexture & 0xFFFF);
		if (roughnessTextureIndex != 0)
		{
			roughness = texture(uTextures[roughnessTextureIndex - 1], vTexCoord).y;
		}
		lightingParams.roughness = roughness;
	}
	
	
	
	vec3 result = vec3(0.0);
	
	// ambient
	result += 1.0 * lightingParams.albedo;
	
	const vec2 pixelCoord = gl_FragCoord.xy;
	const mat4 invViewMatrix = transpose(mat4(uPushConsts.invViewMatrixRow0,
												uPushConsts.invViewMatrixRow1,
												uPushConsts.invViewMatrixRow2,
												vec4(0.0, 0.0, 0.0, 1.0)));
	
	// directional lights
	uint directionalLightCount = uPushConsts.pointLightdirectionalLightCount & 0xFFFF;
	for (uint i = 0; i < directionalLightCount; ++i)
	{
		const DirectionalLightData directionalLightData = uDirectionalLightData[i];
		const vec3 contribution = evaluateDirectionalLight(lightingParams, directionalLightData);
		const float shadow = directionalLightData.shadowDataCount > 0 ?
			evaluateDirectionalLightShadow(directionalLightData, uShadowTexture, invViewMatrix, lightingParams.viewSpacePosition, pixelCoord)
			: 0.0;
		
		result += contribution * (1.0 - shadow);
	}
	
	// point lights
	uint pointLightCount = uPushConsts.pointLightdirectionalLightCount >> 16;
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
		const uint address = getTileAddress(uvec2(gl_FragCoord.xy), uPushConsts.width, wordCount);
		
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

	oResult = vec4(result, 0.5);
}


#version 450

#extension GL_KHR_shader_subgroup_ballot : enable
#extension GL_KHR_shader_subgroup_arithmetic : enable
#extension GL_EXT_nonuniform_qualifier : enable

#ifndef SSAO_ENABLED
#define SSAO_ENABLED 0
#endif // SSAO_ENABLED

#include "lighting_bindings.h"

layout(constant_id = DIRECTIONAL_LIGHT_COUNT_CONST_ID) const uint cDirectionalLightCount = 0;
layout(constant_id = IRRADIANCE_VOLUME_WIDTH_CONST_ID) const uint cGridWidth = 64;
layout(constant_id = IRRADIANCE_VOLUME_HEIGHT_CONST_ID) const uint cGridHeight = 32;
layout(constant_id = IRRADIANCE_VOLUME_DEPTH_CONST_ID) const uint cGridDepth = 64;
layout(constant_id = IRRADIANCE_VOLUME_CASCADES_CONST_ID) const uint cCascades = 3;
layout(constant_id = IRRADIANCE_VOLUME_BASE_SCALE_CONST_ID) const float cGridBaseScale = 2.0;
layout(constant_id = IRRADIANCE_VOLUME_PROBE_SIDE_LENGTH_CONST_ID) const uint cProbeSideLength = 8;
layout(constant_id = IRRADIANCE_VOLUME_DEPTH_PROBE_SIDE_LENGTH_CONST_ID) const uint cDepthProbeSideLength = 16;

layout(set = DEPTH_IMAGE_SET, binding = DEPTH_IMAGE_BINDING) uniform sampler2D uDepthImage;
layout(set = UV_IMAGE_SET, binding = UV_IMAGE_BINDING) uniform sampler2D uUVImage;
layout(set = DDXY_LENGTH_IMAGE_SET, binding = DDXY_LENGTH_IMAGE_BINDING) uniform sampler2D uDdxyLengthImage;
layout(set = DDXY_ROT_MATERIAL_ID_IMAGE_SET, binding = DDXY_ROT_MATERIAL_ID_IMAGE_BINDING) uniform usampler2D uDdxyRotMaterialIdImage;
layout(set = TANGENT_SPACE_IMAGE_SET, binding = TANGENT_SPACE_IMAGE_BINDING) uniform usampler2D uTangentSpaceImage;
layout(set = DEFERRED_SHADOW_IMAGE_SET, binding = DEFERRED_SHADOW_IMAGE_BINDING) uniform sampler2D uDeferredShadowImage;
layout(set = IRRADIANCE_VOLUME_IMAGE_SET, binding = IRRADIANCE_VOLUME_IMAGE_BINDING) uniform sampler2D uIrradianceVolumeImage;
layout(set = IRRADIANCE_VOLUME_DEPTH_IMAGE_SET, binding = IRRADIANCE_VOLUME_DEPTH_IMAGE_BINDING) uniform sampler2D uIrradianceVolumeDepthImage;
#if SSAO_ENABLED
layout(set = OCCLUSION_IMAGE_SET, binding = OCCLUSION_IMAGE_BINDING) uniform sampler2D uOcclusionImage;
#endif // SSAO_ENABLED

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

layout(location = OUT_RESULT) out vec4 oResult;

#define SHADOW_FUNCTIONS 0
#include "commonLighting.h"

const vec3 kRadarColors[14] = 
{
    {0,0.9255,0.9255},   // cyan
    {0,0.62745,0.9647},  // light blue
    {0,0,0.9647},        // blue
    {0,1,0},             // bright green
    {0,0.7843,0},        // green
    {0,0.5647,0},        // dark green
    {1,1,0},             // yellow
    {0.90588,0.75294,0}, // yellow-orange
    {1,0.5647,0},        // orange
    {1,0,0},             // bright red
    {0.8392,0,0},        // red
    {0.75294,0,0},       // dark red
    {1,0,1},             // magenta
    {0.6,0.3333,0.7882}, // purple
};

vec3 convertNumberOfLightsToRadarColor(uint nNumLightsInThisTile, uint uMaxNumLightsPerTile)
{
    // black for no lights
    if( nNumLightsInThisTile == 0 ) return vec3(0.0);
    // light purple for reaching the max
    else if( nNumLightsInThisTile == uMaxNumLightsPerTile ) return vec3(0.847,0.745,0.921);
    // white for going over the max
    else if ( nNumLightsInThisTile > uMaxNumLightsPerTile ) return vec3(1.0);
    // else use weather radar colors
    else
    {
        // use a log scale to provide more detail when the number of lights is smaller

        // want to find the base b such that the logb of uMaxNumLightsPerTile is 14
        // (because we have 14 radar colors)
        float fLogBase = exp2(0.07142857f*log2(float(uMaxNumLightsPerTile)));

        // change of base
        // logb(x) = log2(x) / log2(b)
        uint nColorIndex = uint(floor(log2(float(nNumLightsInThisTile)) / log2(fLogBase)));
        return kRadarColors[nColorIndex];
    }
}

//vec3 sampleAmbientCube(vec3 N, vec3 tc, uint cascadeIndex)
//{
//	vec3 nSquared = N * N;
//	vec3 isNegative = mix(vec3(0.0), vec3(0.5), lessThan(N, vec3(0.0)));
//	tc = tc.xzy;
//	tc.z *= 0.5;
//	vec3 tcz = tc.zzz + isNegative;
//	tcz = tcz * (1.0 / cCascades) + float(cascadeIndex) / cCascades;
//	
//	return nSquared.x * textureLod(uIrradianceVolumeImages[0], vec3(tc.xy, tcz.x), 0).rgb +
//			nSquared.y * textureLod(uIrradianceVolumeImages[1], vec3(tc.xy, tcz.y), 0).rgb +
//			nSquared.z * textureLod(uIrradianceVolumeImages[2], vec3(tc.xy, tcz.z), 0).rgb;
//}

float signNotZero(in float k) 
{
    return k >= 0.0 ? 1.0 : -1.0;
}

vec2 signNotZero(in vec2 v) 
{
    return vec2( signNotZero(v.x), signNotZero(v.y) );
}

vec3 decodeNormal(in vec2 enc) 
{
    vec3 v = vec3(enc.x, enc.y, 1.0 - abs(enc.x) - abs(enc.y));
    if (v.z < 0) 
	{
        v.xy = (1.0 - abs(v.yx)) * signNotZero(v.xy);
    }
    return normalize(v);
}

vec4 decodeDerivatives(in uint encodedDerivativesRot, in vec2 derivativesLength)
{
	vec2 derivativesRot;
	derivativesRot.x = float(encodedDerivativesRot.r & 0x7F) * (1.0 / 127.0);
	derivativesRot.y = float((encodedDerivativesRot >> 8) & 0x7F)  * (1.0 / 127.0);
	derivativesRot = derivativesRot * 2.0 - 1.0;
	float signX = (((encodedDerivativesRot.r >> 7u) & 0x1) == 0) ? 1.0 : -1.0;
	float signY = (((encodedDerivativesRot.r >> 15u) & 0x1) == 0) ? 1.0 : -1.0;
	vec4 derivatives;
	derivatives.x = derivativesRot.x;
	derivatives.y = sqrt(1.0 - derivativesRot.x * derivativesRot.x) * signX;
	derivatives.z = derivativesRot.y;
	derivatives.w = sqrt(1.0 - derivativesRot.y * derivativesRot.y) * signY;
	derivatives.xy *= derivativesLength.x;
	derivatives.zw *= derivativesLength.y;
	return derivatives;
}

mat3 decodeTBN(in uvec4 encodedTBN)
{
	mat3 tbn;
	// octahedron normal vector decoding
	tbn[2] = decodeNormal((encodedTBN.xy * (1.0 / 1023.0)) * 2.0 - 1.0);
	
	// get reference vector
	vec3 refVector;
	uint compIndex = (encodedTBN.z & 0x3);
	if (compIndex == 0)
	{
		refVector = vec3(1.0, 0.0, 0.0);
	}
	else if (compIndex == 1)
	{
		refVector = vec3(0.0, 1.0, 0.0);
	}
	else
	{
		refVector = vec3(0.0, 0.0, 1.0);
	}
	
	// decode tangent
	uint cosAngleUint = ((encodedTBN.z >> 2u) & 0xFF);
	float cosAngle = (cosAngleUint * (1.0 / 255.0)) * 2.0 - 1.0;
	float sinAngle = sqrt(clamp(1.0 - (cosAngle * cosAngle), 0.0, 1.0));
	sinAngle = ((encodedTBN.w & 0x2) == 0) ? -sinAngle : sinAngle;
	vec3 orthoA = normalize(cross(tbn[2], refVector));
	vec3 orthoB = cross(tbn[2], orthoA);
	tbn[0] = (cosAngle * orthoA) + (sinAngle * orthoB);
	
	// decode bitangent
	tbn[1] = cross(tbn[2], tbn[0]);
	tbn[1] = ((encodedTBN.w & 0x1) == 0) ? tbn[1] : -tbn[1];
	
	return tbn;
}

float square(float v)
{
	return v * v;
}

vec2 octEncode(in vec3 v) 
{
    float l1norm = abs(v.x) + abs(v.y) + abs(v.z);
    vec2 result = v.xy * (1.0/l1norm);
    if (v.z < 0.0) 
	{
        result = (1.0 - abs(result.yx)) * signNotZero(result.xy);
    }
    return result;
}

vec2 texCoordFromDir(vec3 dir, ivec3 probeCoord, int cascade, int probeSideLength)
{
	vec2 texelCoord = vec2((ivec2(cGridWidth, cGridDepth) * ivec2(probeCoord.y, cascade) + probeCoord.xz) * (probeSideLength + 2)) + 1.0;
	texelCoord += (octEncode(dir) * 0.5 + 0.5) * float(probeSideLength);
	const vec2 texelSize = 1.0 / vec2(ivec2(cGridWidth, cGridDepth) * ivec2(cGridHeight, cCascades) * ivec2(probeSideLength + 2));
	return texelCoord  * texelSize;
}

void main() 
{
	const float depth = texelFetch(uDepthImage, ivec2(gl_FragCoord.xy), 0).x;
	
	if (depth == 0.0)
	{
		oResult = vec4(vec3(0.529, 0.808, 0.922), 1.0);
		return;
	}
	
	// get uv, derivatives, material id and tangent frame
	const vec2 ddxyLength = texelFetch(uDdxyLengthImage, ivec2(gl_FragCoord.xy), 0).xy;
	const uvec2 ddxyRotMaterialId = texelFetch(uDdxyRotMaterialIdImage, ivec2(gl_FragCoord.xy), 0).xy;
	const uint ddxyRot = ddxyRotMaterialId.x;
	const uint materialId = ddxyRotMaterialId.y;
	const vec4 derivatives = decodeDerivatives(ddxyRot, ddxyLength);
	const uvec4 encodedTBN = texelFetch(uTangentSpaceImage, ivec2(gl_FragCoord.xy), 0).xyzw;
	const mat3 tbn = decodeTBN(encodedTBN);
	const vec2 uv = texelFetch(uUVImage, ivec2(gl_FragCoord.xy), 0).xy;
	
	const MaterialData materialData = uMaterialData[materialId];
	
	LightingParams lightingParams;
	
	// albedo
	{
		vec3 albedo = unpackUnorm4x8(materialData.albedoOpacity).rgb;
		uint albedoTextureIndex = (materialData.albedoNormalTexture & 0xFFFF0000) >> 16;
		if (albedoTextureIndex != 0)
		{
			vec4 albedoTexSample = textureGrad(sampler2D(uTextures[nonuniformEXT(albedoTextureIndex - 1)], uSamplers[SAMPLER_LINEAR_REPEAT]), uv, derivatives.xy, derivatives.zw).rgba;
			albedo = albedoTexSample.rgb;
		}
		lightingParams.albedo = accurateSRGBToLinear(albedo);
	}
	
	// normal
	{
		vec3 normal = tbn[2];
		uint normalTextureIndex = (materialData.albedoNormalTexture & 0xFFFF);
		if (normalTextureIndex != 0)
		{
			vec3 tangentSpaceNormal;
			tangentSpaceNormal.xy = textureGrad(sampler2D(uTextures[nonuniformEXT(normalTextureIndex - 1)], uSamplers[SAMPLER_LINEAR_REPEAT]), uv, derivatives.xy, derivatives.zw).xy * 2.0 - 1.0;
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
			metalness = textureGrad(sampler2D(uTextures[nonuniformEXT(metalnessTextureIndex - 1)], uSamplers[SAMPLER_LINEAR_REPEAT]), uv, derivatives.xy, derivatives.zw).z;
		}
		lightingParams.metalness = metalness;
	}
	
	// roughness
	{
		float roughness = unpackUnorm4x8(materialData.metalnessRoughness).y;
		uint roughnessTextureIndex = (materialData.metalnessRoughnessTexture & 0xFFFF);
		if (roughnessTextureIndex != 0)
		{
			roughness = textureGrad(sampler2D(uTextures[nonuniformEXT(roughnessTextureIndex - 1)], uSamplers[SAMPLER_LINEAR_REPEAT]), uv, derivatives.xy, derivatives.zw).y;
		}
		lightingParams.roughness = roughness;
	}
	
	// view space position
	const vec4 clipSpacePosition = vec4(vec2(vTexCoord) * 2.0 - 1.0, depth, 1.0);
	vec4 viewSpacePosition = uPushConsts.invJitteredProjectionMatrix * clipSpacePosition;
	viewSpacePosition /= viewSpacePosition.w;
	
	lightingParams.V = -normalize(viewSpacePosition.xyz);
	lightingParams.viewSpacePosition = viewSpacePosition.xyz;
	
	vec3 result = vec3(0.0);
	
	// ambient
	{
		float visibility = 1.0;
#if SSAO_ENABLED
		visibility = texelFetch(uOcclusionImage, ivec2(gl_FragCoord.xy), 0).x;
#endif // SSAO_ENABLED
		
		mat4 invViewMatrix = transpose(mat4(uPushConsts.invViewMatrixRow0, uPushConsts.invViewMatrixRow1, uPushConsts.invViewMatrixRow2, vec4(0.0, 0.0, 0.0, 1.0)));
		vec4 worldSpacePos4 = invViewMatrix * vec4(lightingParams.viewSpacePosition, 1.0);
		vec3 worldSpacePos = worldSpacePos4.xyz / worldSpacePos4.w;
		vec3 worldSpaceNormal = (invViewMatrix * vec4(lightingParams.N, 0.0)).xyz;
		const ivec3 gridSize = ivec3(cGridWidth, cGridHeight, cGridDepth);
		float currentGridScale = cGridBaseScale;
		uint cascadeIndex = 0;
		
		// search for cascade index
		for (; cascadeIndex < cCascades; ++cascadeIndex)
		{
			// calculate coordinate in world space fixed coordinate system (scaled to voxel size)
			ivec3 coord = ivec3(worldSpacePos * currentGridScale);
			ivec3 offset = ivec3(round(vec3(invViewMatrix[3]) * currentGridScale) - (gridSize / 2));

			// if coordinate is inside grid, we found the correct cascade
			if (all(greaterThanEqual(coord, offset)) && all(lessThan(coord, gridSize + offset - 1)))
			{
				break;
			}
			currentGridScale *= 0.5;
		}
		
		// if cascade was found, calculate diffuse indirect light
		if (cascadeIndex < cCascades)
		{
			vec3 cameraOffset = round(vec3(invViewMatrix[3]) * currentGridScale) - (gridSize / 2);
			vec3 pointGridCoord = worldSpacePos * currentGridScale - cameraOffset;
			ivec3 baseCoord = ivec3(pointGridCoord);
			vec3 worldSpaceViewDir = normalize(invViewMatrix[3].xyz - worldSpacePos);
			vec4 sum = vec4(0.0);
			for (int i = 0; i < 8; ++i)
			{
				ivec3 probeGridCoord = baseCoord + (ivec3(i, i >> 1, i >> 2) & ivec3(1));
				vec3 trilinear =  1.0 - abs(vec3(probeGridCoord) - pointGridCoord);
				float weight = 1.0;
				
				const vec3 trueDirToProbe = vec3(probeGridCoord) - pointGridCoord;
				const bool probeInPoint = dot(trueDirToProbe, trueDirToProbe) < 0.0001;
				
				// smooth backface test
				{
					weight *= probeInPoint ? 1.0 : square(max(0.0001, (dot(normalize(trueDirToProbe), worldSpaceNormal) + 1.0) * 0.5)) + 0.2;
				}
				
				
				// moment visibility test
				if (!probeInPoint)
				{
					vec3 worldSpaceProbePos = vec3(probeGridCoord + cameraOffset) / currentGridScale;
					vec3 biasedProbeToPoint = worldSpacePos - worldSpaceProbePos;// + (worldSpaceNormal + 3.0 * worldSpaceViewDir); // * normalBias
					vec3 dir = normalize(biasedProbeToPoint);
					vec2 texCoord = texCoordFromDir(dir, probeGridCoord, int(cascadeIndex), int(cDepthProbeSideLength));
					float distToProbe = length(biasedProbeToPoint);
					
					vec2 temp = textureLod(uIrradianceVolumeDepthImage, texCoord, 0).xy;
					float mean = temp.x;
					float variance = abs(temp.x * temp.x - temp.y);
					
					float chebyshevWeight = variance / (variance + square(max(distToProbe - mean, 0.0)));
					
					chebyshevWeight = max(chebyshevWeight * chebyshevWeight * chebyshevWeight, 0.0);
					
					weight *= (distToProbe <= mean) ? 1.0 : 0.0;//chebyshevWeight;
				}
				
				// avoid zero weight
				weight = max(0.000001, weight);
				
				const float crushThreshold = 0.2;
				if (weight < crushThreshold)
				{
					weight *= weight * weight * (1.0 / square(crushThreshold));
				}
				
				// trilinear
				weight *= trilinear.x * trilinear.y * trilinear.z;
				
				probeGridCoord = ivec3(fract((probeGridCoord + cameraOffset) / vec3(gridSize)) * gridSize);
				
				vec2 probeTexCoord = texCoordFromDir(worldSpaceNormal, probeGridCoord, int(cascadeIndex), int(cProbeSideLength));
				vec3 probeIrradiance = textureLod(uIrradianceVolumeImage, probeTexCoord, 0).rgb;
				
				probeIrradiance = sqrt(probeIrradiance);
				
				sum += vec4(probeIrradiance * weight, weight);
			}
			vec3 irradiance = sum.xyz / sum.w;
			
			irradiance = irradiance * irradiance;
			
			result += irradiance * lightingParams.albedo * visibility * (1.0 / PI);
		}
		else
		{
			result += lightingParams.albedo * visibility * (1.0 / PI);
		}
	}
	
	// directional lights
	{
		// deferred shadows
		const uint deferredShadowCount = min(4, cDirectionalLightCount);
		const vec4 deferredShadowValues = texelFetch(uDeferredShadowImage, ivec2(gl_FragCoord.xy), 0);
		for (uint i = 0; i < deferredShadowCount; ++i)
		{
			const DirectionalLightData directionalLightData = uDirectionalLightData[i];
			const vec3 contribution = evaluateDirectionalLight(lightingParams, directionalLightData);
			result += contribution * (1.0 - deferredShadowValues[i]);
		}
	}
	
	// point lights
	if (uPushConsts.pointLightCount > 0)
	{
		uint wordMin = 0;
		const uint wordCount = (uPushConsts.pointLightCount + 31) / 32;
		uint wordMax = wordCount - 1;
		
		const uint zBinAddress = clamp(uint(floor(-viewSpacePosition.z)), 0, 8191);
		const uint zBinData = uPointLightZBins[zBinAddress];
		const uint minIndex = (zBinData & uint(0xFFFF0000)) >> 16;
		const uint maxIndex = zBinData & uint(0xFFFF);
		// mergedMin scalar from this point
		const uint mergedMin = subgroupBroadcastFirst(subgroupMin(minIndex)); 
		// mergedMax scalar from this point
		const uint mergedMax = subgroupBroadcastFirst(subgroupMax(maxIndex)); 
		wordMin = max(mergedMin / 32, wordMin);
		wordMax = min(mergedMax / 32, wordMax);
		const uint address = getTileAddress(uvec2(gl_FragCoord.xy), uint(textureSize(uDepthImage, 0).x), wordCount);
		
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


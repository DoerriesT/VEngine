#version 450

#extension GL_EXT_nonuniform_qualifier : enable

#include "voxelization_bindings.h"
#include "common.h"

layout(constant_id = DIRECTIONAL_LIGHT_COUNT_CONST_ID) const uint cDirectionalLightCount = 0;
layout(constant_id = VOXEL_GRID_WIDTH_CONST_ID) const uint cVoxelGridWidth = 128;
layout(constant_id = VOXEL_GRID_HEIGHT_CONST_ID) const uint cVoxelGridHeight = 64;
layout(constant_id = VOXEL_GRID_DEPTH_CONST_ID) const uint cVoxelGridDepth = 128;
layout(constant_id = IRRADIANCE_VOLUME_WIDTH_CONST_ID) const uint cGridWidth = 64;
layout(constant_id = IRRADIANCE_VOLUME_HEIGHT_CONST_ID) const uint cGridHeight = 32;
layout(constant_id = IRRADIANCE_VOLUME_DEPTH_CONST_ID) const uint cGridDepth = 64;
layout(constant_id = IRRADIANCE_VOLUME_CASCADES_CONST_ID) const uint cCascades = 3;
layout(constant_id = IRRADIANCE_VOLUME_BASE_SCALE_CONST_ID) const float cGridBaseScale = 2.0;

layout(set = IRRADIANCE_VOLUME_IMAGE_SET, binding = IRRADIANCE_VOLUME_IMAGE_BINDING) uniform sampler3D uIrradianceVolumeImages[3];
layout(set = VOXEL_IMAGE_SET, binding = VOXEL_IMAGE_BINDING, rgba16f) uniform image3D uVoxelSceneImage;
layout(set = OPACITY_IMAGE_SET, binding = OPACITY_IMAGE_BINDING, r8) uniform image3D uOpacityImage;

layout(set = MATERIAL_DATA_SET, binding = MATERIAL_DATA_BINDING) readonly buffer MATERIAL_DATA 
{
    MaterialData uMaterialData[];
};

layout(set = DIRECTIONAL_LIGHT_DATA_SET, binding = DIRECTIONAL_LIGHT_DATA_BINDING) readonly buffer DIRECTIONAL_LIGHT_DATA
{
	DirectionalLightData uDirectionalLightData[];
};

layout(set = SHADOW_MATRICES_SET, binding = SHADOW_MATRICES_BINDING) readonly buffer SHADOW_MATRICES
{
	mat4 uShadowMatrices[];
};

layout(set = SHADOW_IMAGE_SET, binding = SHADOW_IMAGE_BINDING) uniform texture2DArray uShadowImage;
layout(set = SHADOW_SAMPLER_SET, binding = SHADOW_SAMPLER_BINDING) uniform sampler uShadowSampler;

layout(set = TEXTURES_SET, binding = TEXTURES_BINDING) uniform texture2D uTextures[TEXTURE_ARRAY_SIZE];
layout(set = SAMPLERS_SET, binding = SAMPLERS_BINDING) uniform sampler uSamplers[SAMPLER_COUNT];

layout(push_constant) uniform PUSH_CONSTS 
{
	PushConsts uPushConsts;
};

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec3 vWorldPos;
layout(location = 3) flat in uint vMaterialIndex;

#define DIFFUSE_ONLY 1
#define SHADOW_FUNCTIONS 0
#include "commonLighting.h"

vec3 sampleAmbientCube(vec3 N, vec3 tc, uint cascadeIndex)
{
	vec3 nSquared = N * N;
	vec3 isNegative = mix(vec3(0.0), vec3(0.5), lessThan(N, vec3(0.0)));
	tc = tc.xzy;
	tc.z *= 0.5;
	vec3 tcz = tc.zzz + isNegative;
	tcz = tcz * (1.0 / cCascades) + float(cascadeIndex) / cCascades;
	
	return nSquared.x * textureLod(uIrradianceVolumeImages[0], vec3(tc.xy, tcz.x), 0).rgb +
			nSquared.y * textureLod(uIrradianceVolumeImages[1], vec3(tc.xy, tcz.y), 0).rgb +
			nSquared.z * textureLod(uIrradianceVolumeImages[2], vec3(tc.xy, tcz.z), 0).rgb;
}

float square(float v)
{
	return v * v;
}

vec3 sampleAmbientCubeCascade(vec3 worldSpacePos, vec3 worldSpaceNormal, uint cascadeIndex)
{
	vec4 sum = vec4(0.0);
	for (int i = 0; i < 8; ++i)
	{
		vec3 pointGridCoord = worldSpacePos * (cGridBaseScale / (1 << cascadeIndex));
		vec3 probeGridCoord = floor(pointGridCoord) + (ivec3(i, i >> 1, i >> 2) & ivec3(1));
		vec3 trilinear =  1.0 - abs(probeGridCoord - pointGridCoord);
		float weight = 1.0;
		
		const vec3 trueDirToProbe = probeGridCoord - pointGridCoord;
		const bool probeInPoint = dot(trueDirToProbe, trueDirToProbe) < 1e-6;
		
		// smooth backface test
		{
			weight *= probeInPoint ? 1.0 : square(max(0.0001, (dot(normalize(trueDirToProbe), worldSpaceNormal) + 1.0) * 0.5)) + 0.2;
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
		
		vec3 probeIrradiance = sampleAmbientCube(worldSpaceNormal, fract(probeGridCoord / vec3(cGridWidth, cGridHeight, cGridDepth)), cascadeIndex);
		
		probeIrradiance = sqrt(probeIrradiance);
		
		sum += vec4(probeIrradiance * weight, weight);
	}
	vec3 irradiance = sum.xyz / sum.w;
	
	return irradiance * irradiance;
}

vec3 sampleAmbientCubeVolume(vec3 worldSpacePos, vec3 worldSpaceNormal)
{
	const ivec3 gridSize = ivec3(cGridWidth, cGridHeight, cGridDepth);
	float currentGridScale = cGridBaseScale;
	uint cascadeIndex = 0;
	
	// search for cascade index
	for (; cascadeIndex < cCascades; ++cascadeIndex)
	{
		// calculate coordinate in world space fixed coordinate system (scaled to voxel size)
		ivec3 coord = ivec3(worldSpacePos * currentGridScale);
		ivec3 offset = ivec3(round(vec3(uPushConsts.invViewMatrix[3]) * currentGridScale) - (gridSize / 2));

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
		return sampleAmbientCubeCascade(worldSpacePos, worldSpaceNormal, cascadeIndex);
	}
	else
	{
		return vec3(0.18);
	}
}

void main() 
{
	const MaterialData materialData = uMaterialData[vMaterialIndex];
	
	vec3 albedo = vec3(0.0);
	vec3 N = normalize(vNormal);
	
	// albedo
	{
		albedo = unpackUnorm4x8(materialData.albedoOpacity).rgb;
		uint albedoTextureIndex = (materialData.albedoNormalTexture & 0xFFFF0000) >> 16;
		if (albedoTextureIndex != 0)
		{
			vec4 albedoTexSample = texture(sampler2D(uTextures[nonuniformEXT(albedoTextureIndex - 1)], uSamplers[SAMPLER_LINEAR_REPEAT]), vTexCoord).rgba;
			albedo = albedoTexSample.rgb;
		}
		albedo = accurateSRGBToLinear(albedo);
	}
	const vec3 worldSpacePos = vWorldPos;
	
	vec3 result = vec3(0.0);
	
	// ambient
	result += sampleAmbientCubeVolume(worldSpacePos, N) * albedo * (1.0 / PI);
	
	// directional lights
	for (uint lightIdx = 0; lightIdx < cDirectionalLightCount; ++lightIdx)
	{
		const DirectionalLightData lightData = uDirectionalLightData[lightIdx];
		const uint shadowDataOffsetCount = lightData.shadowDataOffsetCount;
		const uint offset = shadowDataOffsetCount >> 16;
		const uint end = (shadowDataOffsetCount & 0xFFFF) + offset;
		
		vec4 shadowCoord = vec4(0.0);
		uint shadowMapIndex = offset;
		// find cascade
		for (; shadowMapIndex < end; ++shadowMapIndex)
		{
			shadowCoord = uShadowMatrices[shadowMapIndex] * vec4(worldSpacePos, 1.0);
			if (any(greaterThan(abs(shadowCoord.xy), vec2(1.0))))
			{
				continue;
			}
			shadowCoord.xy = shadowCoord.xy * 0.5 + 0.5;
			break;
		}
		
		float shadow = 0.0;
		if (shadowMapIndex < end)
		{
			shadowCoord.z -= 0.0001;
			shadow = texture(sampler2DArrayShadow(uShadowImage, uShadowSampler), vec4(shadowCoord.xy, shadowMapIndex, shadowCoord.z)).x;
		}
		
		
		const vec3 L = mat3(uPushConsts.invViewMatrix) * lightData.direction.xyz;
		const float NdotL = max(dot(N, L), 0.0);
		result += (1.0 - shadow) * albedo * (1.0 / PI) * NdotL * lightData.color.rgb;
	}
	
	const vec3 gridSize = vec3(cVoxelGridWidth, cVoxelGridHeight, cVoxelGridDepth);
	ivec3 coord = ivec3(round(vWorldPos * uPushConsts.voxelScale));
	if (all(greaterThanEqual(coord, ivec3(uPushConsts.gridOffset.xyz))) && all(lessThan(coord, gridSize + ivec3(uPushConsts.gridOffset.xyz))))
	{
		coord = ivec3(fract(coord / gridSize) * gridSize);
		// cascades are stacked on top of each other inside the same image
		coord += ivec3(0, uPushConsts.cascade * cVoxelGridHeight, 0);
		// z and y axis are switched in order to "grow" the image along the z axis with each additional cascade
		coord = coord.xzy;
	
		vec3 voxel = imageLoad(uVoxelSceneImage, coord).rgb;
		float opacity = imageLoad(uOpacityImage, coord).x;
		float weight = opacity == 0.0 ? 1.0 : 0.03;
		result = mix(voxel.rgb, result, weight);
		
		imageStore(uVoxelSceneImage, coord, vec4(result, 1.0));
		imageStore(uOpacityImage, coord, vec4(1.0));
	}
}


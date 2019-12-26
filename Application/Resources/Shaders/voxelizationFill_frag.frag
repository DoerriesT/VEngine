#version 450

#extension GL_EXT_nonuniform_qualifier : enable

#include "voxelizationFill_bindings.h"
#include "common.h"

layout(constant_id = DIRECTIONAL_LIGHT_COUNT_CONST_ID) const uint cDirectionalLightCount = 0;
layout(constant_id = VOXEL_GRID_WIDTH_CONST_ID) const uint cVoxelGridWidth = 128;
layout(constant_id = VOXEL_GRID_HEIGHT_CONST_ID) const uint cVoxelGridHeight = 64;
layout(constant_id = VOXEL_GRID_DEPTH_CONST_ID) const uint cVoxelGridDepth = 128;
layout(constant_id = VOXEL_SCALE_CONST_ID) const float cVoxelScale = 1.0;

layout(set = VOXEL_PTR_IMAGE_SET, binding = VOXEL_PTR_IMAGE_BINDING) uniform usampler3D uVoxelPtrImage;
layout(set = BIN_VIS_IMAGE_BUFFER_SET, binding = BIN_VIS_IMAGE_BUFFER_BINDING, r32ui) uniform uimageBuffer uBinVisImageBuffer;
layout(set = COLOR_IMAGE_BUFFER_SET, binding = COLOR_IMAGE_BUFFER_BINDING, rgba16f) uniform imageBuffer uColorImageBuffer;

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
	result += albedo * (1.0 / PI);
	
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
	ivec3 coord = ivec3(round(vWorldPos * cVoxelScale));
	ivec3 brickCoord = ivec3(floor(vec3(coord) / 16.0));
	
	if (all(greaterThanEqual(brickCoord, ivec3(uPushConsts.gridOffset.xyz))) && all(lessThan(brickCoord, gridSize + ivec3(uPushConsts.gridOffset.xyz))))
	{
		ivec3 localCoord = ivec3(fract(vec3(coord) / 16.0) * 16.0);
		ivec3 cubeCoord = localCoord / 4;
		ivec3 localCubeCoord = ivec3(fract(cubeCoord / 4.0) * 4.0);
		uint cubeIdx = localCubeCoord.x + localCubeCoord.z * 4 + localCubeCoord.y * 16;
		ivec3 bitCoord = ivec3(fract(localCoord / 4.0) * 4.0);
		uint bitIdx = bitCoord.x + bitCoord.z * 4 + bitCoord.y * 16;
		bool upper = bitIdx > 31;
		bitIdx = upper ? bitIdx - 32 : bitIdx;
		
		const vec3 brickGridSize = vec3(128, 64, 128);
		const vec3 invBrickGridSize = 1.0 / brickGridSize;
		
		ivec3 brickCoord = ivec3(fract(floor(vec3(coord) / 16.0) * invBrickGridSize) * brickGridSize);
		uint brickPtr = texelFetch(uVoxelPtrImage, brickCoord, 0).x;
		
		if (brickPtr != 0)
		{
			--brickPtr;
			imageAtomicOr(uBinVisImageBuffer, int(brickPtr * 128 + cubeIdx * 2 + (upper ? 1 : 0)), (1u << bitIdx));
			vec3 prevColor = imageLoad(uColorImageBuffer, int(brickPtr * 64 + cubeIdx)).rgb;
			imageStore(uColorImageBuffer, int(brickPtr * 64 + cubeIdx), vec4(mix(result, prevColor, 0.98), 1.0));
		}
	}
}


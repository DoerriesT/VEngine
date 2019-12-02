#version 450

#extension GL_EXT_nonuniform_qualifier : enable

#include "voxelization_bindings.h"
#include "common.h"

layout(constant_id = DIRECTIONAL_LIGHT_COUNT_CONST_ID) const uint cDirectionalLightCount = 0;
layout(constant_id = VOXEL_GRID_WIDTH_CONST_ID) const uint cVoxelGridWidth = 128;
layout(constant_id = VOXEL_GRID_HEIGHT_CONST_ID) const uint cVoxelGridHeight = 64;
layout(constant_id = VOXEL_GRID_DEPTH_CONST_ID) const uint cVoxelGridDepth = 128;

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
	//result += albedo * (1.0 / PI);
	
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


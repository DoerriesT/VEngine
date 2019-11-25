#version 450

#extension GL_EXT_nonuniform_qualifier : enable

#include "voxelization_bindings.h"
#include "common.h"

layout(constant_id = VOXEL_GRID_WIDTH_CONST_ID) const uint cVoxelGridWidth = 128;
layout(constant_id = VOXEL_GRID_HEIGHT_CONST_ID) const uint cVoxelGridHeight = 64;
layout(constant_id = VOXEL_GRID_DEPTH_CONST_ID) const uint cVoxelGridDepth = 128;

layout(set = VOXEL_IMAGE_SET, binding = VOXEL_IMAGE_BINDING, rgba16f) uniform image3D uVoxelSceneImage;
layout(set = OPACITY_IMAGE_SET, binding = OPACITY_IMAGE_BINDING, r8) uniform image3D uOpacityImage;

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
layout(location = 2) in vec3 vWorldPos;
layout(location = 3) flat in uint vMaterialIndex;

#define DIFFUSE_ONLY 1
#define SHADOW_FUNCTIONS 0
#include "commonLighting.h"

void main() 
{
	const MaterialData materialData = uMaterialData[vMaterialIndex];
	
	LightingParams lightingParams;
	
	// albedo
	{
		vec3 albedo = unpackUnorm4x8(materialData.albedoOpacity).rgb;
		uint albedoTextureIndex = (materialData.albedoNormalTexture & 0xFFFF0000) >> 16;
		if (albedoTextureIndex != 0)
		{
			vec4 albedoTexSample = texture(sampler2D(uTextures[nonuniformEXT(albedoTextureIndex - 1)], uSamplers[SAMPLER_LINEAR_REPEAT]), vTexCoord).rgba;
			albedo = albedoTexSample.rgb;
		}
		lightingParams.albedo = accurateSRGBToLinear(albedo);
	}
	
	vec3 result = vec3(0.0);
	
	// ambient
	{
		result += lightingParams.albedo;
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
	
		imageStore(uVoxelSceneImage, coord, vec4(result, 1.0));
		imageStore(uOpacityImage, coord, vec4(1.0));
	}
}


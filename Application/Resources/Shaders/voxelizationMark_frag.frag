#version 450

#include "voxelizationMark_bindings.h"

layout(constant_id = BRICK_VOLUME_WIDTH_CONST_ID) const uint cBrickVolumeWidth = 128;
layout(constant_id = BRICK_VOLUME_HEIGHT_CONST_ID) const uint cBrickVolumeHeight = 64;
layout(constant_id = BRICK_VOLUME_DEPTH_CONST_ID) const uint cBrickVolumeDepth = 128;
layout(constant_id = INV_VOXEL_BRICK_SIZE_CONST_ID) const float cInvVoxelBrickSize = 0.625;
layout(constant_id = VOXEL_SCALE_CONST_ID) const float cVoxelScale = 16.0;

layout(set = MARK_IMAGE_SET, binding = MARK_IMAGE_BINDING, r8ui) uniform uimage3D uMarkImage;

layout(push_constant) uniform PUSH_CONSTS 
{
	PushConsts uPushConsts;
};

layout(location = 0) in vec3 vWorldPos;


void main() 
{
	const vec3 gridSize = vec3(cBrickVolumeWidth, cBrickVolumeHeight, cBrickVolumeDepth);
	ivec3 coord = ivec3(round(vWorldPos * cVoxelScale));
	coord = ivec3(floor(vec3(coord) * cInvVoxelBrickSize));
	if (all(greaterThanEqual(coord, ivec3(uPushConsts.gridOffset.xyz))) && all(lessThan(coord, gridSize + ivec3(uPushConsts.gridOffset.xyz))))
	{
		coord = ivec3(fract(coord / gridSize) * gridSize);
		
		imageStore(uMarkImage, coord, uvec4(1));
	}
}


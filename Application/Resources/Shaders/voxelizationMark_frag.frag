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
	vec3 coord = floor(vWorldPos * cVoxelScale * cInvVoxelBrickSize);
	if (all(greaterThanEqual(coord, uPushConsts.gridOffset.xyz)) && all(lessThan(coord, gridSize + uPushConsts.gridOffset.xyz)))
	{
		coord = floor(fract(coord / gridSize) * gridSize);
		
		imageStore(uMarkImage, ivec3(coord), uvec4(1));
	}
}


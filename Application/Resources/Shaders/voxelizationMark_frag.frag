#version 450

#include "voxelizationMark_bindings.h"
#include "common.h"

layout(constant_id = VOXEL_GRID_WIDTH_CONST_ID) const uint cVoxelGridWidth = 128;
layout(constant_id = VOXEL_GRID_HEIGHT_CONST_ID) const uint cVoxelGridHeight = 64;
layout(constant_id = VOXEL_GRID_DEPTH_CONST_ID) const uint cVoxelGridDepth = 128;
layout(constant_id = VOXEL_SCALE_CONST_ID) const float cVoxelScale = 1.0;

layout(set = VOXEL_PTR_IMAGE_SET, binding = VOXEL_PTR_IMAGE_BINDING, r32ui) uniform uimage3D uVoxelPtrImage;

layout(push_constant) uniform PUSH_CONSTS 
{
	PushConsts uPushConsts;
};

layout(location = 0) in vec3 vWorldPos;


void main() 
{
	const vec3 gridSize = vec3(cVoxelGridWidth, cVoxelGridHeight, cVoxelGridDepth);
	ivec3 coord = ivec3(round(vWorldPos * cVoxelScale));
	coord = ivec3(floor(vec3(coord) / 16.0));
	if (all(greaterThanEqual(coord, ivec3(uPushConsts.gridOffset.xyz))) && all(lessThan(coord, gridSize + ivec3(uPushConsts.gridOffset.xyz))))
	{
		coord = ivec3(fract(coord / gridSize) * gridSize);
		
		imageAtomicCompSwap(uVoxelPtrImage, coord, 0, 0xFFFFFFFF);
	}
}


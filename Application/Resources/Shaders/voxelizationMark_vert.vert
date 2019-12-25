#version 450

#include "voxelizationMark_bindings.h"

layout(set = VERTEX_POSITIONS_SET, binding = VERTEX_POSITIONS_BINDING) readonly buffer VERTEX_POSITIONS 
{
    float uPositions[];
};

layout(set = TRANSFORM_DATA_SET, binding = TRANSFORM_DATA_BINDING) readonly buffer TRANSFORM_DATA 
{
    mat4 uTransformData[];
};

layout(push_constant) uniform PUSH_CONSTS 
{
	PushConsts uPushConsts;
};

void main() 
{
	const mat4 modelMatrix = uTransformData[uPushConsts.transformIndex];
	const vec3 position = vec3(uPositions[gl_VertexIndex * 3 + 0], uPositions[gl_VertexIndex * 3 + 1], uPositions[gl_VertexIndex * 3 + 2]);
    
	gl_Position = modelMatrix * vec4(position, 1.0);
}


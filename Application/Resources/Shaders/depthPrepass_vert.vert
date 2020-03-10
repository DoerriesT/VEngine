#version 450

#include "depthPrepass_bindings.h"
#include "common.h"

#ifndef ALPHA_MASK_ENABLED
#define ALPHA_MASK_ENABLED 0
#endif // ALPHA_MASK_ENABLED

layout(set = VERTEX_POSITIONS_SET, binding = VERTEX_POSITIONS_BINDING) readonly buffer VERTEX_POSITIONS 
{
    float uPositions[];
};

#if ALPHA_MASK_ENABLED
layout(set = VERTEX_TEXCOORDS_SET, binding = VERTEX_TEXCOORDS_BINDING) readonly buffer VERTEX_TEXCOORDS 
{
    float uTexCoords[];
};
#endif // ALPHA_MASK_ENABLED

layout(set = INSTANCE_DATA_SET, binding = INSTANCE_DATA_BINDING) readonly buffer INSTANCE_DATA 
{
    SubMeshInstanceData uInstanceData[];
};

layout(set = TRANSFORM_DATA_SET, binding = TRANSFORM_DATA_BINDING) readonly buffer TRANSFORM_DATA 
{
    mat4 uTransformData[];
};

struct SubMeshData
{
	uint indexCount;
	uint firstIndex;
	int vertexOffset;
};

layout(set = SUB_MESH_DATA_SET, binding = SUB_MESH_DATA_BINDING) readonly buffer SUB_MESH_DATA 
{
    SubMeshData uSubMeshData[];
};

layout(push_constant) uniform PUSH_CONSTS 
{
	PushConsts uPushConsts;
};

#if ALPHA_MASK_ENABLED
layout(location = 0) out vec2 vTexCoord;
layout(location = 1) flat out uint vMaterialIndex;
#endif // ALPHA_MASK_ENABLED

void main() 
{
	//uint instanceIndex = gl_VertexIndex >> 16;
	//SubMeshInstanceData instanceData = uInstanceData[instanceIndex];
	uint vertexIndex = gl_VertexIndex;//uint vertexIndex = (gl_VertexIndex & 0xFFFF) + uSubMeshData[instanceData.subMeshIndex].vertexOffset;
	
	const mat4 modelMatrix = uTransformData[uPushConsts.transformIndex];
	const vec3 position = vec3(uPositions[vertexIndex * 3 + 0], uPositions[vertexIndex * 3 + 1], uPositions[vertexIndex * 3 + 2]);
    gl_Position = uPushConsts.jitteredViewProjectionMatrix * modelMatrix * vec4(position, 1.0);									
	
#if ALPHA_MASK_ENABLED
	vTexCoord = vec2(uTexCoords[vertexIndex * 2 + 0], uTexCoords[vertexIndex * 2 + 1]);
	vMaterialIndex= uPushConsts.materialIndex;
#endif // ALPHA_MASK_ENABLED
}


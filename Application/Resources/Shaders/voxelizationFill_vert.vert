#version 450

#include "common.h"
#include "voxelizationFill_bindings.h"

#ifndef LIGHTING_ENABLED
#define LIGHTING_ENABLED 0
#endif // LIGHTING_ENABLED

layout(set = VERTEX_POSITIONS_SET, binding = VERTEX_POSITIONS_BINDING) readonly buffer VERTEX_POSITIONS 
{
    float uPositions[];
};

#if LIGHTING_ENABLED
layout(set = VERTEX_NORMALS_SET, binding = VERTEX_NORMALS_BINDING) readonly buffer VERTEX_NORMALS 
{
    float uNormals[];
};

layout(set = VERTEX_TEXCOORDS_SET, binding = VERTEX_TEXCOORDS_BINDING) readonly buffer VERTEX_TEXCOORDS 
{
    float uTexCoords[];
};
#endif // LIGHTING_ENABLED

layout(set = TRANSFORM_DATA_SET, binding = TRANSFORM_DATA_BINDING) readonly buffer TRANSFORM_DATA 
{
    mat4 uTransformData[];
};

layout(push_constant) uniform PUSH_CONSTS 
{
	PushConsts uPushConsts;
};

layout(constant_id = VOXEL_SCALE_CONST_ID) const float cVoxelScale = 16.0;

#if LIGHTING_ENABLED
layout(location = 0) out vec2 vTexCoord;
layout(location = 1) out vec3 vNormal;
layout(location = 2) flat out uint vMaterialIndex;
#endif // LIGHTING_ENABLED

void main() 
{
	//uint instanceIndex = gl_VertexIndex >> 16;
	//SubMeshInstanceData instanceData = uInstanceData[instanceIndex];
	uint vertexIndex = gl_VertexIndex;//(gl_VertexIndex & 0xFFFF) + uSubMeshData[instanceData.subMeshIndex].vertexOffset;
	
	const mat4 modelMatrix = uTransformData[uPushConsts.transformIndex];
	const vec3 position = vec3(uPositions[vertexIndex * 3 + 0], uPositions[vertexIndex * 3 + 1], uPositions[vertexIndex * 3 + 2]);
#if LIGHTING_ENABLED
	vTexCoord = vec2(uTexCoords[vertexIndex * 2 + 0], uTexCoords[vertexIndex * 2 + 1]);
	vNormal = mat3(modelMatrix) * vec3(uNormals[vertexIndex * 3 + 0], uNormals[vertexIndex * 3 + 1], uNormals[vertexIndex * 3 + 2]);
	vMaterialIndex = uPushConsts.materialIndex;
#endif // LIGHTING_ENABLED

	gl_Position = modelMatrix * vec4(position, 1.0);
	gl_Position.xyz = gl_Position.xyz * cVoxelScale - (uPushConsts.gridOffset.xyz);
}


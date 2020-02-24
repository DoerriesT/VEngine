#version 450

#include "shadows_bindings.h"

struct SubMeshData
{
	uint indexCount;
	uint firstIndex;
	int vertexOffset;
};

#ifndef ALPHA_MASK_ENABLED
#define ALPHA_MASK_ENABLED 0
#endif // ALPHA_MASK_ENABLED

layout(set = INSTANCE_DATA_SET, binding = INSTANCE_DATA_BINDING) readonly buffer INSTANCE_DATA 
{
    SubMeshInstanceData uInstanceData[];
};

layout(set = SUB_MESH_DATA_SET, binding = SUB_MESH_DATA_BINDING) readonly buffer SUB_MESH_DATA 
{
    SubMeshData uSubMeshData[];
};

layout(set = VERTEX_POSITIONS_SET, binding = VERTEX_POSITIONS_BINDING) readonly buffer VERTEX_POSITIONS 
{
    float uPositions[];
};

layout(set = TRANSFORM_DATA_SET, binding = TRANSFORM_DATA_BINDING) readonly buffer TRANSFORM_DATA 
{
    mat4 uTransformData[];
};

layout(set = MATERIAL_DATA_SET, binding = MATERIAL_DATA_BINDING) readonly buffer MATERIAL_DATA 
{
    MaterialData uMaterialData[];
};

layout(set = VERTEX_TEXCOORDS_SET, binding = VERTEX_TEXCOORDS_BINDING) readonly buffer VERTEX_TEXCOORDS 
{
    float uTexCoords[];
};

layout(push_constant) uniform PUSH_CONSTS 
{
	PushConsts uPushConsts;
};

#if ALPHA_MASK_ENABLED
layout(location = 0) out vec2 vTexCoord;
layout(location = 1) flat out uint vAlbedoTextureIndex;
#endif //ALPHA_MASK_ENABLED

void main() 
{
	//uint instanceIndex = gl_VertexIndex >> 16;
	//SubMeshInstanceData instanceData = uInstanceData[instanceIndex];
	uint vertexIndex = gl_VertexIndex;//(gl_VertexIndex & 0xFFFF) + uSubMeshData[instanceData.subMeshIndex].vertexOffset;
	
	const vec3 position = vec3(uPositions[vertexIndex * 3 + 0], uPositions[vertexIndex * 3 + 1], uPositions[vertexIndex * 3 + 2]);
    gl_Position = uPushConsts.shadowMatrix * uTransformData[uPushConsts.transformIndex] * vec4(position, 1.0);
	
	#if ALPHA_MASK_ENABLED
	vTexCoord = vec2(uTexCoords[vertexIndex * 2 + 0], uTexCoords[vertexIndex * 2 + 1]);
	vAlbedoTextureIndex = (uMaterialData[uPushConsts.materialIndex].albedoNormalTexture & 0xFFFF0000) >> 16;
	#endif //ALPHA_MASK_ENABLED
}


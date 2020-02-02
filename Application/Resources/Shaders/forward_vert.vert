#version 450

#include "forward_bindings.h"
#include "common.h"

layout(set = VERTEX_POSITIONS_SET, binding = VERTEX_POSITIONS_BINDING) readonly buffer VERTEX_POSITIONS 
{
    float uPositions[];
};

layout(set = VERTEX_NORMALS_SET, binding = VERTEX_NORMALS_BINDING) readonly buffer VERTEX_NORMALS 
{
    float uNormals[];
};

layout(set = VERTEX_TEXCOORDS_SET, binding = VERTEX_TEXCOORDS_BINDING) readonly buffer VERTEX_TEXCOORDS 
{
    float uTexCoords[];
};

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

layout(location = 0) out vec2 vTexCoord;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec4 vWorldPos;
layout(location = 3) flat out uint vMaterialIndex;

void main() 
{
	uint instanceIndex = gl_VertexIndex >> 16;
	SubMeshInstanceData instanceData = uInstanceData[instanceIndex];
	uint vertexIndex = (gl_VertexIndex & 0xFFFF) + uSubMeshData[instanceData.subMeshIndex].vertexOffset;
	
	const mat4 modelMatrix = uTransformData[instanceData.transformIndex];
	const vec3 position = vec3(uPositions[vertexIndex * 3 + 0], uPositions[vertexIndex * 3 + 1], uPositions[vertexIndex * 3 + 2]);
    gl_Position = uPushConsts.jitteredViewProjectionMatrix * modelMatrix * vec4(position, 1.0);
	const mat4 viewMatrix = transpose(mat4(uPushConsts.viewMatrixRow0, uPushConsts.viewMatrixRow1, uPushConsts.viewMatrixRow2, vec4(0.0, 0.0, 0.0, 1.0)));
	const mat4 modelViewMatrix = viewMatrix * modelMatrix;										
	vTexCoord = vec2(uTexCoords[vertexIndex * 2 + 0], uTexCoords[vertexIndex * 2 + 1]);
	vNormal = mat3(modelViewMatrix) * vec3(uNormals[vertexIndex * 3 + 0], uNormals[vertexIndex * 3 + 1], uNormals[vertexIndex * 3 + 2]);
	vWorldPos = modelViewMatrix * vec4(position, 1.0);
	vMaterialIndex= instanceData.materialIndex;
}


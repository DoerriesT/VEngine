#include "common.h"

#define INSTANCE_DATA_SET 0
#define INSTANCE_DATA_BINDING 0
#define TRANSFORM_DATA_SET 0
#define TRANSFORM_DATA_BINDING 1
#define MATERIAL_DATA_SET 0
#define MATERIAL_DATA_BINDING 2
#define VERTEX_POSITIONS_SET 0
#define VERTEX_POSITIONS_BINDING 3
#define VERTEX_NORMALS_SET 0
#define VERTEX_NORMALS_BINDING 4
#define VERTEX_TEXCOORDS_SET 0
#define VERTEX_TEXCOORDS_BINDING 5
#define SUB_MESH_DATA_SET 0
#define SUB_MESH_DATA_BINDING 6

#define TEXTURES_SET 1
#define TEXTURES_BINDING 0
#define SAMPLERS_SET 1
#define SAMPLERS_BINDING 1

#define OUT_UV 0
#define OUT_DDXY_LENGTH 1
#define OUT_DDXY_ROTATION_MATERIAL_ID 2
#define OUT_TANGENT_SPACE 3

struct PushConsts
{
	mat4 jitteredViewProjectionMatrix;
	vec4 viewMatrixRow0;
	vec4 viewMatrixRow1;
	vec4 viewMatrixRow2;
	uint transformIndex;
	uint materialIndex;
};
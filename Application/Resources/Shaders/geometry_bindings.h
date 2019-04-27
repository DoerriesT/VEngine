#include "common.h"

#define TRANSFORM_DATA_SET 0
#define TRANSFORM_DATA_BINDING 0
#define MATERIAL_DATA_SET 0
#define MATERIAL_DATA_BINDING 1
#define TEXTURES_SET 1
#define TEXTURES_BINDING 0

#define OUT_ALBEDO 0
#define OUT_NORMAL 1
#define OUT_METALNESS_ROUGHNESS_OCCLUSION 2

struct PushConsts
{
	mat4 jitteredViewProjectionMatrix;
	vec4 viewMatrixRow0;
	vec4 viewMatrixRow1;
	vec4 viewMatrixRow2;
	uint transformIndex;
	uint materialIndex;
};
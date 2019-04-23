#include "common.h"

#define CONSTANT_DATA_SET 0
#define CONSTANT_DATA_BINDING 0
#define TRANSFORM_DATA_SET 0
#define TRANSFORM_DATA_BINDING 1
#define MATERIAL_DATA_SET 0
#define MATERIAL_DATA_BINDING 2
#define TEXTURES_SET 1
#define TEXTURES_BINDING 0

#define OUT_ALBEDO 0
#define OUT_NORMAL 1
#define OUT_METALNESS_ROUGHNESS_OCCLUSION 2

struct ConstantData
{
	mat4 viewMatrix;
	mat4 jitteredViewProjectionMatrix;
};

struct PushConsts
{
	uint transformIndex;
	uint materialIndex;
};
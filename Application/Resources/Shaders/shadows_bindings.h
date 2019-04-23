#include "common.h"

#define TRANSFORM_DATA_SET 0
#define TRANSFORM_DATA_BINDING 0
#define MATERIAL_DATA_SET 0
#define MATERIAL_DATA_BINDING 1
#define TEXTURES_SET 1
#define TEXTURES_BINDING 0

struct PushConsts
{
	mat4 viewProjectionMatrix;
	uint transformIndex;
	uint materialIndex;
};
#include "common.h"

#define INSTANCE_DATA_SET 0
#define INSTANCE_DATA_BINDING 0
#define TRANSFORM_DATA_SET 0
#define TRANSFORM_DATA_BINDING 1
#define MATERIAL_DATA_SET 0
#define MATERIAL_DATA_BINDING 2
#define SHADOW_ATLAS_SET 0
#define SHADOW_ATLAS_BINDING 3
#define DIRECTIONAL_LIGHT_DATA_SET 0
#define DIRECTIONAL_LIGHT_DATA_BINDING 4
#define POINT_LIGHT_DATA_SET 0
#define POINT_LIGHT_DATA_BINDING 5
#define SHADOW_DATA_SET 0
#define SHADOW_DATA_BINDING 6
#define POINT_LIGHT_Z_BINS_SET 0
#define POINT_LIGHT_Z_BINS_BINDING 7
#define POINT_LIGHT_MASK_SET 0
#define POINT_LIGHT_MASK_BINDING 8

#define TEXTURES_SET 1
#define TEXTURES_BINDING 0

#define OUT_RESULT 0

struct PushConsts
{
	mat4 jitteredViewProjectionMatrix;
	vec4 invViewMatrixRow0;
	vec4 invViewMatrixRow1;
	vec4 invViewMatrixRow2;
	uint pointLightdirectionalLightCount;
	uint width;
};
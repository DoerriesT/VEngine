#include "common.h"

#define RESULT_IMAGE_SET 0
#define RESULT_IMAGE_BINDING 0
#define DEPTH_IMAGE_SET 0
#define DEPTH_IMAGE_BINDING 1
#define UV_IMAGE_SET 0
#define UV_IMAGE_BINDING 2
#define DDXY_LENGTH_IMAGE_SET 0
#define DDXY_LENGTH_IMAGE_BINDING 3
#define DDXY_ROT_MATERIAL_ID_IMAGE_SET 0
#define DDXY_ROT_MATERIAL_ID_IMAGE_BINDING 4
#define TANGENT_SPACE_IMAGE_SET 0
#define TANGENT_SPACE_IMAGE_BINDING 5
#define SHADOW_ATLAS_SET 0
#define SHADOW_ATLAS_BINDING 6
#define DEFERRED_SHADOW_IMAGE_SET 0
#define DEFERRED_SHADOW_IMAGE_BINDING 7
#define DIRECTIONAL_LIGHT_DATA_SET 0
#define DIRECTIONAL_LIGHT_DATA_BINDING 8
#define POINT_LIGHT_DATA_SET 0
#define POINT_LIGHT_DATA_BINDING 9
#define SHADOW_MATRICES_SET 0
#define SHADOW_MATRICES_BINDING 10
#define POINT_LIGHT_Z_BINS_SET 0
#define POINT_LIGHT_Z_BINS_BINDING 11
#define POINT_LIGHT_MASK_SET 0
#define POINT_LIGHT_MASK_BINDING 12
#define OCCLUSION_IMAGE_SET 0
#define OCCLUSION_IMAGE_BINDING 13
#define MATERIAL_DATA_SET 0
#define MATERIAL_DATA_BINDING 14

#define TEXTURES_SET 1
#define TEXTURES_BINDING 0

#define OUT_RESULT 0

#define DIRECTIONAL_LIGHT_COUNT_CONST_ID 0

struct PushConsts
{
	mat4 invJitteredProjectionMatrix;
	vec4 invViewMatrixRow0;
	vec4 invViewMatrixRow1;
	vec4 invViewMatrixRow2;
	uint pointLightCount;
};
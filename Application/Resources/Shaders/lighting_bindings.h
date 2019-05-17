#include "common.h"

#define RESULT_IMAGE_SET 0
#define RESULT_IMAGE_BINDING 0
#define GBUFFER_IMAGES_SET 0
#define GBUFFER_IMAGES_BINDING 1
#define SHADOW_ATLAS_SET 0
#define SHADOW_ATLAS_BINDING 2
#define DIRECTIONAL_LIGHT_DATA_SET 0
#define DIRECTIONAL_LIGHT_DATA_BINDING 3
#define POINT_LIGHT_DATA_SET 0
#define POINT_LIGHT_DATA_BINDING 4
#define SHADOW_DATA_SET 0
#define SHADOW_DATA_BINDING 5
#define POINT_LIGHT_Z_BINS_SET 0
#define POINT_LIGHT_Z_BINS_BINDING 6
#define POINT_LIGHT_MASK_SET 0
#define POINT_LIGHT_MASK_BINDING 7
#define OCCLUSION_IMAGE_SET 0
#define OCCLUSION_IMAGE_BINDING 8

#define DEPTH_TEXTURE_INDEX 0
#define ALBEDO_TEXTURE_INDEX 1
#define NORMAL_TEXTURE_INDEX 2
#define MRO_TEXTURE_INDEX 3

struct PushConsts
{
	mat4 invJitteredProjectionMatrix;
	vec4 invViewMatrixRow0;
	vec4 invViewMatrixRow1;
	vec4 invViewMatrixRow2;
	uint pointLightCount;
	uint directionalLightCount;
};
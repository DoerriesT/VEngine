#include "common.h"

#define BRICK_PTR_IMAGE_SET 0
#define BRICK_PTR_IMAGE_BINDING 0
#define COLOR_IMAGE_BUFFER_SET 0
#define COLOR_IMAGE_BUFFER_BINDING 1
#define DEPTH_IMAGE_SET 0
#define DEPTH_IMAGE_BINDING 2
#define UV_IMAGE_SET 0
#define UV_IMAGE_BINDING 3
#define DDXY_LENGTH_IMAGE_SET 0
#define DDXY_LENGTH_IMAGE_BINDING 4
#define DDXY_ROT_MATERIAL_ID_IMAGE_SET 0
#define DDXY_ROT_MATERIAL_ID_IMAGE_BINDING 5
#define TANGENT_SPACE_IMAGE_SET 0
#define TANGENT_SPACE_IMAGE_BINDING 6
#define DEFERRED_SHADOW_IMAGE_SET 0
#define DEFERRED_SHADOW_IMAGE_BINDING 7
#define DIRECTIONAL_LIGHT_DATA_SET 0
#define DIRECTIONAL_LIGHT_DATA_BINDING 8
#define POINT_LIGHT_DATA_SET 0
#define POINT_LIGHT_DATA_BINDING 9
#define POINT_LIGHT_Z_BINS_SET 0
#define POINT_LIGHT_Z_BINS_BINDING 10
#define POINT_LIGHT_MASK_SET 0
#define POINT_LIGHT_MASK_BINDING 11
#define MATERIAL_DATA_SET 0
#define MATERIAL_DATA_BINDING 12
#define IRRADIANCE_VOLUME_IMAGE_SET 0
#define IRRADIANCE_VOLUME_IMAGE_BINDING 13
#define IRRADIANCE_VOLUME_DEPTH_IMAGE_SET 0
#define IRRADIANCE_VOLUME_DEPTH_IMAGE_BINDING 14


#define TEXTURES_SET 1
#define TEXTURES_BINDING 0
#define SAMPLERS_SET 1
#define SAMPLERS_BINDING 1

#define DIRECTIONAL_LIGHT_COUNT_CONST_ID 0
#define BRICK_VOLUME_WIDTH_CONST_ID 1
#define BRICK_VOLUME_HEIGHT_CONST_ID 2
#define BRICK_VOLUME_DEPTH_CONST_ID 3
#define INV_VOXEL_BRICK_SIZE_CONST_ID 4
#define VOXEL_SCALE_CONST_ID 5
#define BRICK_SCALE_CONST_ID 6
#define BIN_VIS_BRICK_SIZE_CONST_ID 7
#define COLOR_BRICK_SIZE_CONST_ID 8
#define IRRADIANCE_VOLUME_WIDTH_CONST_ID 9
#define IRRADIANCE_VOLUME_HEIGHT_CONST_ID 10
#define IRRADIANCE_VOLUME_DEPTH_CONST_ID 11
#define IRRADIANCE_VOLUME_CASCADES_CONST_ID 12
#define IRRADIANCE_VOLUME_BASE_SCALE_CONST_ID 13
#define IRRADIANCE_VOLUME_PROBE_SIDE_LENGTH_CONST_ID 14
#define IRRADIANCE_VOLUME_DEPTH_PROBE_SIDE_LENGTH_CONST_ID 15

struct PushConsts
{
	mat4 invJitteredProjectionMatrix;
	vec4 invViewMatrixRow0;
	vec4 invViewMatrixRow1;
	vec4 invViewMatrixRow2;
	uint pointLightCount;
	float time;
	float deltaTime;
};
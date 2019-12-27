#define QUEUE_BUFFER_SET 0
#define QUEUE_BUFFER_BINDING 0
#define RESULT_IMAGE_SET 0
#define RESULT_IMAGE_BINDING 1
#define DISTANCE_IMAGE_SET 0
#define DISTANCE_IMAGE_BINDING 2
#define BRICK_PTR_IMAGE_SET 0
#define BRICK_PTR_IMAGE_BINDING 3
#define COLOR_IMAGE_BUFFER_SET 0
#define COLOR_IMAGE_BUFFER_BINDING 4
#define BIN_VIS_IMAGE_BUFFER_SET 0
#define BIN_VIS_IMAGE_BUFFER_BINDING 5


#define GRID_WIDTH_CONST_ID 0
#define GRID_HEIGHT_CONST_ID 1
#define GRID_DEPTH_CONST_ID 2
#define CASCADES_CONST_ID 3
#define GRID_BASE_SCALE_CONST_ID 4
#define BRICK_VOLUME_WIDTH_CONST_ID 5
#define BRICK_VOLUME_HEIGHT_CONST_ID 6
#define BRICK_VOLUME_DEPTH_CONST_ID 7
#define VOXEL_GRID_WIDTH_CONST_ID 8
#define VOXEL_GRID_HEIGHT_CONST_ID 9
#define VOXEL_GRID_DEPTH_CONST_ID 10
#define VOXEL_SCALE_CONST_ID 11
#define BIN_VIS_BRICK_SIZE_CONST_ID 12
#define COLOR_BRICK_SIZE_CONST_ID 13

struct PushConsts
{
	vec4 cameraPosition;
	ivec3 voxelGridOffset;
	float time;
};
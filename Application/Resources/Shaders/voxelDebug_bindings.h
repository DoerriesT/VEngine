#define VOXEL_IMAGE_SET 0
#define VOXEL_IMAGE_BINDING 0

#define VOXEL_GRID_WIDTH_CONST_ID 0
#define VOXEL_GRID_HEIGHT_CONST_ID 1
#define VOXEL_GRID_DEPTH_CONST_ID 2
#define VOXEL_BASE_SCALE_CONST_ID 3
#define VOXEL_CASCADES_CONST_ID 4

struct PushConsts
{
	mat4 jitteredViewProjectionMatrix;
	vec4 cameraPosition;
	float scale;
	uint cascadeIndex;
};
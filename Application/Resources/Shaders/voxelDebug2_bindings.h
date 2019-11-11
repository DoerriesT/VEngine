#define VOXEL_IMAGE_SET 0
#define VOXEL_IMAGE_BINDING 0
#define RESULT_IMAGE_SET 0
#define RESULT_IMAGE_BINDING 1
#define OPACITY_IMAGE_SET 0
#define OPACITY_IMAGE_BINDING 2

#define VOXEL_GRID_WIDTH_CONST_ID 0
#define VOXEL_GRID_HEIGHT_CONST_ID 1
#define VOXEL_GRID_DEPTH_CONST_ID 2
#define WIDTH_CONST_ID 3
#define HEIGHT_CONST_ID 4
#define TEXEL_WIDTH_CONST_ID 5
#define TEXEL_HEIGHT_CONST_ID 6

struct PushConsts
{
	mat4 invViewProjection;
	vec3 cameraPos;
	int cascade;
	float voxelScale;
};
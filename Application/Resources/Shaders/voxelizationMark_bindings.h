#define TRANSFORM_DATA_SET 0
#define TRANSFORM_DATA_BINDING 0
#define VERTEX_POSITIONS_SET 0
#define VERTEX_POSITIONS_BINDING 1
#define MARK_IMAGE_SET 0
#define MARK_IMAGE_BINDING 2

#define VOXEL_GRID_WIDTH_CONST_ID 0
#define VOXEL_GRID_HEIGHT_CONST_ID 1
#define VOXEL_GRID_DEPTH_CONST_ID 2
#define VOXEL_SCALE_CONST_ID 3

struct PushConsts
{
	vec3 gridOffset;
	float invGridResolution;
	uint superSamplingFactor;
	uint transformIndex;
};
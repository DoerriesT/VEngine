#define TRANSFORM_DATA_SET 0
#define TRANSFORM_DATA_BINDING 0
#define VERTEX_POSITIONS_SET 0
#define VERTEX_POSITIONS_BINDING 1
#define MARK_IMAGE_SET 0
#define MARK_IMAGE_BINDING 2

#define BRICK_VOLUME_WIDTH_CONST_ID 0
#define BRICK_VOLUME_HEIGHT_CONST_ID 1
#define BRICK_VOLUME_DEPTH_CONST_ID 2
#define INV_VOXEL_BRICK_SIZE_CONST_ID 3
#define VOXEL_SCALE_CONST_ID 4
#define BRICK_SCALE_CONST_ID 5


struct PushConsts
{
	vec3 gridOffset;
	float invGridResolution;
	uint superSamplingFactor;
	uint transformIndex;
};
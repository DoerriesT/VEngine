#define TRANSFORM_DATA_SET 0
#define TRANSFORM_DATA_BINDING 0
#define POSITIONS_SET 0
#define POSITIONS_BINDING 1
#define MARK_IMAGE_SET 0
#define MARK_IMAGE_BINDING 2
#define INDICES_SET 0
#define INDICES_BINDING 3

#define BRICK_GRID_WIDTH_CONST_ID 0
#define BRICK_GRID_HEIGHT_CONST_ID 1
#define BRICK_GRID_DEPTH_CONST_ID 2
#define BRICK_SCALE_CONST_ID 3

struct PushConsts
{
	uint indexOffset;
	uint indexCount;
	uint vertexOffset;
	uint transformIndex;
	vec3 gridOffset;
};
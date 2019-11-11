#define AGE_IMAGE_SET 0
#define AGE_IMAGE_BINDING 0
#define QUEUE_BUFFER_SET 0
#define QUEUE_BUFFER_BINDING 1
#define INDIRECT_CMD_SET 0
#define INDIRECT_CMD_BINDING 2
#define CULLED_BUFFER_SET 0
#define CULLED_BUFFER_BINDING 3
#define HIZ_IMAGE_SET 0
#define HIZ_IMAGE_BINDING 4

#define GRID_WIDTH_CONST_ID 0
#define GRID_HEIGHT_CONST_ID 1
#define GRID_DEPTH_CONST_ID 2
#define CASCADES_CONST_ID 3
#define QUEUE_CAPACITY_CONST_ID 4
#define GRID_BASE_SCALE_CONST_ID 5

struct PushConsts
{
	mat4 viewProjectionMatrix;
	vec4 cameraPos;
	vec2 resolution;
	float time;
};
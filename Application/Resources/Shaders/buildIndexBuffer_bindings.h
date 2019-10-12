#define CLUSTER_INFO_SET 0
#define CLUSTER_INFO_BINDING 0
#define INPUT_INDICES_SET 0
#define INPUT_INDICES_BINDING 1
#define DRAW_COMMAND_SET 0
#define DRAW_COMMAND_BINDING 2
#define FILTERED_INDICES_SET 0
#define FILTERED_INDICES_BINDING 3
#define TRANSFORM_DATA_SET 0
#define TRANSFORM_DATA_BINDING 4
#define POSITIONS_SET 0
#define POSITIONS_BINDING 5

struct PushConsts
{
	mat4 viewProjection;
	vec2 resolution;
	uint clusterOffset;
	uint cullBackface;
};
#define CLUSTER_INFO_SET 0
#define CLUSTER_INFO_BINDING 0
#define INPUT_INDICES_SET 0
#define INPUT_INDICES_BINDING 1
#define INDEX_OFFSETS_SET 0
#define INDEX_OFFSETS_BINDING 2
#define INDEX_COUNTS_SET 0
#define INDEX_COUNTS_BINDING 3
#define FILTERED_INDICES_SET 0
#define FILTERED_INDICES_BINDING 4
#define TRANSFORM_DATA_SET 0
#define TRANSFORM_DATA_BINDING 5
#define POSITIONS_SET 0
#define POSITIONS_BINDING 6
#define VIEW_PROJECTION_MATRIX_SET 0
#define VIEW_PROJECTION_MATRIX_BINDING 7

struct PushConsts
{
	vec2 resolution;
	uint clusterOffset;
	uint cullBackface;
	uint viewProjectionMatrixOffset;
};
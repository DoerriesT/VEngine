#define INDEX_OFFSETS_SET 0
#define INDEX_OFFSETS_BINDING 0
#define INDEX_COUNTS_SET 0
#define INDEX_COUNTS_BINDING 1
#define INSTANCE_DATA_SET 0
#define INSTANCE_DATA_BINDING 2
#define SUB_MESH_DATA_SET 0
#define SUB_MESH_DATA_BINDING 3
#define INDIRECT_BUFFER_SET 0
#define INDIRECT_BUFFER_BINDING 4
#define DRAW_COUNTS_SET 0
#define DRAW_COUNTS_BINDING 5

struct PushConsts
{
	uint drawCallOffset;
	uint instanceOffset;
	uint batchIndex;
	uint drawCallCount;
};
#define FREE_BRICKS_BUFFER_SET 0
#define FREE_BRICKS_BUFFER_BINDING 0

struct PushConsts
{
	uvec4 volumeSize;
	uvec4 offset;
	uint clearNextFree;
};
#define RESULT_IMAGE_BINDING 0
#define INPUT_IMAGE_BINDING 1
#define PREV_RESULT_IMAGE_BINDING 2

struct PushConsts
{
	float2 texelSize;
	uint width;
	uint height;
	uint addPrevious;
};
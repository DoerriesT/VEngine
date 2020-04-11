#define RESULT_IMAGE_SET 0
#define RESULT_IMAGE_BINDING 0
#define INPUT_IMAGE_SET 0
#define INPUT_IMAGE_BINDING 1
#define PREV_RESULT_IMAGE_SET 0
#define PREV_RESULT_IMAGE_BINDING 2
#define SAMPLER_SET 0
#define SAMPLER_BINDING 3

struct PushConsts
{
	float2 texelSize;
	uint width;
	uint height;
	uint addPrevious;
};
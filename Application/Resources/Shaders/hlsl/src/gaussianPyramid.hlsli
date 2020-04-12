#define RESULT_IMAGE_SET 0
#define RESULT_IMAGE_BINDING 0
#define INPUT_IMAGE_SET 0
#define INPUT_IMAGE_BINDING 1
#define LINEAR_SAMPLER_SET 0
#define LINEAR_SAMPLER_BINDING 2

struct PushConsts
{
	float2 srcTexelSize;
	uint width;
	uint height;
	uint horizontal;
};
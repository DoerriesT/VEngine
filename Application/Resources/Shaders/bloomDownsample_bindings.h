#define RESULT_IMAGE_SET 0
#define RESULT_IMAGE_BINDING 0
#define INPUT_IMAGE_SET 0
#define INPUT_IMAGE_BINDING 1
#define SAMPLER_SET 0
#define SAMPLER_BINDING 2

struct PushConsts
{
	vec2 texelSize;
	uint width;
	uint height;
	uint doWeightedAverage;
};
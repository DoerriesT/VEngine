#define RESULT_IMAGE_BINDING 0
#define INPUT_IMAGE_BINDING 1

struct PushConsts
{
	float2 texelSize;
	uint width;
	uint height;
	uint doWeightedAverage;
};
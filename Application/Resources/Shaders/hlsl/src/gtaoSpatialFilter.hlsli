#define INPUT_IMAGE_SET 0
#define INPUT_IMAGE_BINDING 0
#define RESULT_IMAGE_SET 0
#define RESULT_IMAGE_BINDING 1
#define POINT_SAMPLER_SET 0
#define POINT_SAMPLER_BINDING 2

struct PushConsts
{
	float2 texelSize;
	uint width;
	uint height;
};
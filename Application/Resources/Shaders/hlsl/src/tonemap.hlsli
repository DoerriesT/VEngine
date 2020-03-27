#define RESULT_IMAGE_SET 0
#define RESULT_IMAGE_BINDING 0
#define INPUT_IMAGE_SET 0
#define INPUT_IMAGE_BINDING 1
#define BLOOM_IMAGE_SET 0
#define BLOOM_IMAGE_BINDING 2
#define LINEAR_SAMPLER_SET 0
#define LINEAR_SAMPLER_BINDING 3
#define EXPOSURE_DATA_SET 0
#define EXPOSURE_DATA_BINDING 4

struct PushConsts
{
	float2 texelSize;
	uint applyLinearToGamma;
	uint bloomEnabled;
	float bloomStrength;
};
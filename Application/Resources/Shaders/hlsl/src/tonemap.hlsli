#define RESULT_IMAGE_BINDING 0
#define INPUT_IMAGE_BINDING 1
#define BLOOM_IMAGE_BINDING 2
#define EXPOSURE_DATA_BINDING 3

struct PushConsts
{
	float2 texelSize;
	uint applyLinearToGamma;
	uint bloomEnabled;
	float bloomStrength;
};
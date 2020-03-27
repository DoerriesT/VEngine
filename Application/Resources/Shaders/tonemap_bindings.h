#define RESULT_IMAGE_SET 0
#define RESULT_IMAGE_BINDING 0
#define SOURCE_IMAGE_SET 0
#define SOURCE_IMAGE_BINDING 1
#define LUMINANCE_VALUES_SET 0
#define LUMINANCE_VALUES_BINDING 2
#define BLOOM_IMAGE_SET 0
#define BLOOM_IMAGE_BINDING 3
#define POINT_SAMPLER_SET 0
#define POINT_SAMPLER_BINDING 4
#define LINEAR_SAMPLER_SET 0
#define LINEAR_SAMPLER_BINDING 5
#define EXPOSURE_DATA_SET 0
#define EXPOSURE_DATA_BINDING 6

struct PushConsts
{
	uint luminanceIndex;
	uint applyLinearToGamma;
	uint bloomEnabled;
	float bloomStrength;
	float exposureCompensation;
	float exposureMin;
	float exposureMax;
	uint fixExposureToMax;
};
#define RESULT_IMAGE_SET 0
#define RESULT_IMAGE_BINDING 0
#define RAY_HIT_PDF_IMAGE_SET 0
#define RAY_HIT_PDF_IMAGE_BINDING 1
#define MASK_IMAGE_SET 0
#define MASK_IMAGE_BINDING 2
#define DEPTH_IMAGE_SET 0
#define DEPTH_IMAGE_BINDING 3
#define NORMAL_IMAGE_SET 0
#define NORMAL_IMAGE_BINDING 4
#define SPEC_ROUGHNESS_IMAGE_SET 0
#define SPEC_ROUGHNESS_IMAGE_BINDING 5
#define PREV_COLOR_IMAGE_SET 0
#define PREV_COLOR_IMAGE_BINDING 6
#define VELOCITY_IMAGE_SET 0
#define VELOCITY_IMAGE_BINDING 7
#define RESULT_MASK_IMAGE_SET 0
#define RESULT_MASK_IMAGE_BINDING 8
#define LINEAR_SAMPLER_SET 0
#define LINEAR_SAMPLER_BINDING 9
#define CONSTANT_BUFFER_SET 0
#define CONSTANT_BUFFER_BINDING 10
#define CONSTANT_BUFFER_SET 0
#define CONSTANT_BUFFER_BINDING 10
#define EXPOSURE_DATA_BUFFER_SET 0
#define EXPOSURE_DATA_BUFFER_BINDING 11

struct Constants
{
	float4 unprojectParams;
	float2 depthProjectParams;
	int2 jitter;
	uint width;
	uint height;
	float texelWidth;
	float texelHeight;
	float diameterToScreen;
	float bias;
	uint ignoreHistory;
	uint horizontalGap;
	uint verticalGap;
};
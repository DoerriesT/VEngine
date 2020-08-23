#define RESULT_IMAGE_BINDING 0
#define RAY_HIT_PDF_IMAGE_BINDING 1
#define MASK_IMAGE_BINDING 2
#define DEPTH_IMAGE_BINDING 3
#define NORMAL_ROUGHNESS_IMAGE_BINDING 4
#define ALBEDO_METALNESS_IMAGE_BINDING 5
#define PREV_COLOR_IMAGE_BINDING 6
#define VELOCITY_IMAGE_BINDING 7
#define RESULT_MASK_IMAGE_BINDING 8
#define CONSTANT_BUFFER_BINDING 9
#define EXPOSURE_DATA_BUFFER_BINDING 10

struct Constants
{
	float4 unprojectParams;
	float2 depthProjectParams;
	uint width;
	uint height;
	float texelWidth;
	float texelHeight;
	float diameterToScreen;
	float bias;
	uint ignoreHistory;
};
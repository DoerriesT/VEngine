#define RESULT_IMAGE_SET 0
#define RESULT_IMAGE_BINDING 0
#define HISTORY_IMAGE_SET 0
#define HISTORY_IMAGE_BINDING 1
#define COLOR_RAY_DEPTH_IMAGE_SET 0
#define COLOR_RAY_DEPTH_IMAGE_BINDING 2
#define MASK_IMAGE_SET 0
#define MASK_IMAGE_BINDING 3
#define LINEAR_SAMPLER_SET 0
#define LINEAR_SAMPLER_BINDING 4
#define CONSTANT_BUFFER_SET 0
#define CONSTANT_BUFFER_BINDING 5

struct Constants
{
	float4x4 reprojectionMatrix;
	float width;
	float height;
	float texelWidth;
	float texelHeight;
};
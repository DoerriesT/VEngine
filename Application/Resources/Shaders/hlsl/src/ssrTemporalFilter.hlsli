#define RESULT_IMAGE_BINDING 0
#define HISTORY_IMAGE_BINDING 1
#define COLOR_RAY_DEPTH_IMAGE_BINDING 2
#define MASK_IMAGE_BINDING 3
#define CONSTANT_BUFFER_BINDING 4
#define EXPOSURE_DATA_BUFFER_BINDING 5

struct Constants
{
	float4x4 reprojectionMatrix;
	float width;
	float height;
	float texelWidth;
	float texelHeight;
	uint ignoreHistory;
};
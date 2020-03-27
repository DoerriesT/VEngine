#define INPUT_IMAGE_SET 0
#define INPUT_IMAGE_BINDING 0
#define LUMINANCE_HISTOGRAM_SET 0
#define LUMINANCE_HISTOGRAM_BINDING 1
#define EXPOSURE_DATA_BUFFER_SET 0
#define EXPOSURE_DATA_BUFFER_BINDING 2

struct PushConsts
{
	float scale;
	float bias;
	uint width;
};
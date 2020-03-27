#define INPUT_IMAGE_SET 0
#define INPUT_IMAGE_BINDING 0
#define LUMINANCE_HISTOGRAM_SET 0
#define LUMINANCE_HISTOGRAM_BINDING 1

struct PushConsts
{
	float scale;
	float bias;
	uint width;
};
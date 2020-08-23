#define INPUT_IMAGE_BINDING 0
#define RESULT_IMAGE_BINDING 1

struct PushConsts
{
	int2 prevMipSize;
	uint width;
	uint height;
	uint copyOnly;
};
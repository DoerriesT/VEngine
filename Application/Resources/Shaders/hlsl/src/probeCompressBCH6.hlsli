#define RESULT_IMAGE_BINDING 0
#define INPUT_IMAGE_BINDING 7

struct PushConsts
{
	uint mip;
	uint resultRes;
	float texelSize;
};
#define RESULT_IMAGE_BINDING 0
#define INPUT_IMAGE_BINDING 7
#define POINT_SAMPLER_BINDING 14

struct PushConsts
{
	uint mip;
	uint resultRes;
	float texelSize;
};
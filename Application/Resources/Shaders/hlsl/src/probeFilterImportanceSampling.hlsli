#define INPUT_IMAGE_BINDING 0
#define RESULT_IMAGE_BINDING 1
#define LINEAR_SAMPLER_BINDING 11

struct PushConsts
{
	uint mip;
	uint width;
	float mipCount;
	float texelSize;
	float roughness;
};
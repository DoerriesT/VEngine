#define RESULT_IMAGE_SET 0
#define RESULT_IMAGE_BINDING 0
#define SOURCE_IMAGE_SET 0
#define SOURCE_IMAGE_BINDING 1
#define POINT_SAMPLER_SET 0
#define POINT_SAMPLER_BINDING 2

struct PushConsts
{
	uvec4 const0;
	uvec4 const1;
	uint gammaSpaceInput;
};
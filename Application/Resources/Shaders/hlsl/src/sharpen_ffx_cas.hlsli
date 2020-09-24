#define RESULT_IMAGE_BINDING 0
#define INPUT_IMAGE_BINDING 1

struct PushConsts
{
	uint4 const0;
	uint4 const1;
	float2 texelSize;
	uint gammaSpaceInput;
	float time;
};
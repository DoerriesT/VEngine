#define RESULT_IMAGE_SET 0
#define RESULT_IMAGE_BINDING 0
#define DEPTH_IMAGE_SET 0
#define DEPTH_IMAGE_BINDING 1
#define VOLUMETRIC_FOG_IMAGE_SET 0
#define VOLUMETRIC_FOG_IMAGE_BINDING 2
#define LINEAR_SAMPLER_SET 0
#define LINEAR_SAMPLER_BINDING 3

struct PushConsts
{
	float4 unprojectParams;
	uint width;
	uint height;
	float texelWidth;
	float texelHeight;
};
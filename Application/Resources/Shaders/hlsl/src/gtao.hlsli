#define DEPTH_IMAGE_SET 0
#define DEPTH_IMAGE_BINDING 0
#define RESULT_IMAGE_SET 0
#define RESULT_IMAGE_BINDING 1
#define POINT_SAMPLER_SET 0
#define POINT_SAMPLER_BINDING 2

struct PushConsts
{
	float4 unprojectParams;
	float4 resolution;
	float focalLength;
	float radius;
	float maxRadiusPixels;
	float numSteps;
	uint frame;
};
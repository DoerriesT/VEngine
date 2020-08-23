#define DEPTH_IMAGE_BINDING 0
#define RESULT_IMAGE_BINDING 1

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
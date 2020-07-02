#define DEPTH_IMAGE_BINDING 0
#define VOLUMETRIC_FOG_IMAGE_BINDING 1
#define LINEAR_SAMPLER_BINDING 2
#define BLUE_NOISE_IMAGE_BINDING 3
#define RAYMARCHED_VOLUMETRICS_IMAGE_BINDING 4
#define RAYMARCHED_VOLUMETRICS_DEPTH_IMAGE_BINDING 5
#define POINT_SAMPLER_BINDING 6

struct PushConsts
{
	float4 unprojectParams;
	uint frame;
	float texelWidth;
	float texelHeight;
	uint raymarchedFog;
};
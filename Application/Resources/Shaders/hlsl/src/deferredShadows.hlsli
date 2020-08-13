#define RESULT_IMAGE_SET 0
#define RESULT_IMAGE_BINDING 0
#define DEPTH_IMAGE_SET 0
#define DEPTH_IMAGE_BINDING 1
#define SHADOW_IMAGE_SET 0
#define SHADOW_IMAGE_BINDING 2
#define SHADOW_SAMPLER_SET 0
#define SHADOW_SAMPLER_BINDING 3
#define POINT_SAMPLER_SET 0
#define POINT_SAMPLER_BINDING 4
#define SHADOW_MATRICES_SET 0
#define SHADOW_MATRICES_BINDING 5
#define CASCADE_PARAMS_BUFFER_SET 0
#define CASCADE_PARAMS_BUFFER_BINDING 6
#define CONSTANT_BUFFER_SET 0
#define CONSTANT_BUFFER_BINDING 7
#define BLUE_NOISE_IMAGE_BINDING 8
#define FOM_IMAGE_BINDING 9
#define LINEAR_SAMPLER_BINDING 10


struct Constants
{
	float4x4 invViewMatrix;
	float4 unprojectParams;
	float3 direction;
	int cascadeDataOffset;
	int cascadeCount;
	uint width;
	uint height;
	float texelWidth;
	float texelHeight;
	uint frame;
};
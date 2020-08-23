#define RESULT_IMAGE_BINDING 0
#define DEPTH_IMAGE_BINDING 1
#define SHADOW_IMAGE_BINDING 2
#define SHADOW_MATRICES_BINDING 3
#define CASCADE_PARAMS_BUFFER_BINDING 4
#define CONSTANT_BUFFER_BINDING 5
#define BLUE_NOISE_IMAGE_BINDING 6
#define FOM_IMAGE_BINDING 7
#define FOM_DEPTH_RANGE_IMAGE_BINDING 8


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
	uint volumetricShadow;
};
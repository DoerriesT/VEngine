#define RAY_HIT_PDF_IMAGE_BINDING 0
#define MASK_IMAGE_BINDING 1
#define HIZ_PYRAMID_IMAGE_BINDING 2
#define NORMAL_ROUGHNESS_IMAGE_BINDING 3
#define CONSTANT_BUFFER_BINDING 4

struct Constants
{
	float4x4 projectionMatrix;
	float4 unprojectParams;
	int2 subsample;
	float2 noiseScale;
	float2 noiseJitter;
	uint width;
	uint height;
	float texelWidth;
	float texelHeight;
	float hiZMaxLevel;
	uint noiseTexId;
	float bias;
};
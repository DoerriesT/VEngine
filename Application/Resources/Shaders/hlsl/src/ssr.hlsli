#define RAY_HIT_PDF_IMAGE_SET 0
#define RAY_HIT_PDF_IMAGE_BINDING 0
#define MASK_IMAGE_SET 0
#define MASK_IMAGE_BINDING 1
#define HIZ_PYRAMID_IMAGE_SET 0
#define HIZ_PYRAMID_IMAGE_BINDING 2
#define NORMAL_IMAGE_SET 0
#define NORMAL_IMAGE_BINDING 3
#define SPEC_ROUGHNESS_IMAGE_SET 0
#define SPEC_ROUGHNESS_IMAGE_BINDING 4
#define CONSTANT_BUFFER_SET 0
#define CONSTANT_BUFFER_BINDING 5

#define TEXTURES_SET 1
#define TEXTURES_BINDING 0
#define SAMPLERS_SET 1
#define SAMPLERS_BINDING 1

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
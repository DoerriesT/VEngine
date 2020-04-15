#define RESULT_IMAGE_SET 0
#define RESULT_IMAGE_BINDING 0
#define DEPTH_IMAGE_SET 0
#define DEPTH_IMAGE_BINDING 1
#define VOLUMETRIC_FOG_IMAGE_SET 0
#define VOLUMETRIC_FOG_IMAGE_BINDING 2
#define INDIRECT_SPECULAR_LIGHT_IMAGE_SET 0
#define INDIRECT_SPECULAR_LIGHT_IMAGE_BINDING 3
#define BRDF_LUT_IMAGE_SET 0
#define BRDF_LUT_IMAGE_BINDING 4
#define SPEC_ROUGHNESS_IMAGE_SET 0
#define SPEC_ROUGHNESS_IMAGE_BINDING 5
#define NORMAL_IMAGE_SET 0
#define NORMAL_IMAGE_BINDING 6
#define LINEAR_SAMPLER_SET 0
#define LINEAR_SAMPLER_BINDING 7

#define TEXTURES_SET 1
#define TEXTURES_BINDING 0
#define SAMPLERS_SET 1
#define SAMPLERS_BINDING 1

struct PushConsts
{
	float4 unprojectParams;
	float2 noiseScale;
	float2 noiseJitter;
	uint noiseTexId;
	uint width;
	uint height;
	float texelWidth;
	float texelHeight;
};
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
#define ALBEDO_METALNESS_IMAGE_SET 0
#define ALBEDO_METALNESS_IMAGE_BINDING 5
#define NORMAL_ROUGHNESS_IMAGE_SET 0
#define NORMAL_ROUGHNESS_IMAGE_BINDING 6
#define LINEAR_SAMPLER_SET 0
#define LINEAR_SAMPLER_BINDING 7
#define REFLECTION_PROBE_IMAGE_SET 0
#define REFLECTION_PROBE_IMAGE_BINDING 8
#define REFLECTION_PROBE_DATA_SET 0
#define REFLECTION_PROBE_DATA_BINDING 9
#define REFLECTION_PROBE_BIT_MASK_SET 0
#define REFLECTION_PROBE_BIT_MASK_BINDING 10
#define REFLECTION_PROBE_Z_BINS_SET 0
#define REFLECTION_PROBE_Z_BINS_BINDING 11
#define EXPOSURE_DATA_BUFFER_SET 0
#define EXPOSURE_DATA_BUFFER_BINDING 12
#define SSAO_IMAGE_SET 0
#define SSAO_IMAGE_BINDING 13
#define BLUE_NOISE_IMAGE_BINDING 14
#define RAYMARCHED_VOLUMETRICS_IMAGE_BINDING 15

struct PushConsts
{
	float4x4 invViewMatrix;
	float4 unprojectParams;
	uint frame;
	uint width;
	uint height;
	float texelWidth;
	float texelHeight;
	uint probeCount;
	uint ssao;
	uint raymarchedFog;
};
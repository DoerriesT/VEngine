#define DEPTH_IMAGE_BINDING 0
#define INDIRECT_SPECULAR_LIGHT_IMAGE_BINDING 1
#define BRDF_LUT_IMAGE_BINDING 2
#define ALBEDO_METALNESS_IMAGE_BINDING 3
#define NORMAL_ROUGHNESS_IMAGE_BINDING 4
#define REFLECTION_PROBE_IMAGE_BINDING 5
#define REFLECTION_PROBE_DATA_BINDING 6
#define REFLECTION_PROBE_BIT_MASK_BINDING 7
#define REFLECTION_PROBE_Z_BINS_BINDING 8
#define EXPOSURE_DATA_BUFFER_BINDING 9
#define SSAO_IMAGE_BINDING 10

struct PushConsts
{
	float4x4 invViewMatrix;
	float4 unprojectParams;
	float3 cameraPos;
	uint width;
	float texelWidth;
	float texelHeight;
	uint probeCount;
	uint ssao;
};
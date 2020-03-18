#define RESULT_IMAGE_SET 0
#define RESULT_IMAGE_BINDING 0
#define SCATTERING_EXTINCTION_IMAGE_SET 0
#define SCATTERING_EXTINCTION_IMAGE_BINDING 1
#define EMISSIVE_PHASE_IMAGE_SET 0
#define EMISSIVE_PHASE_IMAGE_BINDING 2
#define SHADOW_IMAGE_SET 0
#define SHADOW_IMAGE_BINDING 3
#define SHADOW_SAMPLER_SET 0
#define SHADOW_SAMPLER_BINDING 4
#define SHADOW_MATRICES_SET 0
#define SHADOW_MATRICES_BINDING 5

struct PushConsts
{
	float3 frustumCornerTL;
	uint padding0;
	float3 frustumCornerTR;
	uint padding1;
	float3 frustumCornerBL;
	uint padding2;
	float3 frustumCornerBR;
	int cascadeOffset;
	float3 cameraPos;
	int cascadeCount;
	float3 sunDirection;
	uint padding3;
	float3 sunLightRadiance;
};
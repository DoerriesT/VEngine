#define RESULT_IMAGE_SET 0
#define RESULT_IMAGE_BINDING 0
#define SHADOW_IMAGE_SET 0
#define SHADOW_IMAGE_BINDING 1
#define SHADOW_SAMPLER_SET 0
#define SHADOW_SAMPLER_BINDING 2
#define SHADOW_MATRICES_SET 0
#define SHADOW_MATRICES_BINDING 3

struct PushConsts
{
	float3 frustumCornerTL;
	float scatteringCoefficient;
	float3 frustumCornerTR;
	float absorptionCoefficient;
	float3 frustumCornerBL;
	float phaseAnisotropy;
	float3 frustumCornerBR;
	int cascadeOffset;
	float3 cameraPos;
	int cascadeCount;
	float3 sunDirection;
	uint fogAlbedo;
	float3 sunLightRadiance;
	float density;
};
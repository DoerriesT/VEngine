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
	vec3 frustumCornerTL;
	float scatteringCoefficient;
	vec3 frustumCornerTR;
	float absorptionCoefficient;
	vec3 frustumCornerBL;
	float phaseAnisotropy;
	vec3 frustumCornerBR;
	int cascadeOffset;
	vec3 cameraPos;
	int cascadeCount;
	vec3 sunDirection;
	uint fogAlbedo;
	vec3 sunLightRadiance;
	float density;
};
#define RESULT_IMAGE_SET 0
#define RESULT_IMAGE_BINDING 0
#define PREV_RESULT_IMAGE_SET 0
#define PREV_RESULT_IMAGE_BINDING 1
#define SCATTERING_EXTINCTION_IMAGE_SET 0
#define SCATTERING_EXTINCTION_IMAGE_BINDING 2
#define EMISSIVE_PHASE_IMAGE_SET 0
#define EMISSIVE_PHASE_IMAGE_BINDING 3
#define SHADOW_IMAGE_SET 0
#define SHADOW_IMAGE_BINDING 4
#define SHADOW_SAMPLER_SET 0
#define SHADOW_SAMPLER_BINDING 5
#define SHADOW_MATRICES_SET 0
#define SHADOW_MATRICES_BINDING 6
#define CONSTANT_BUFFER_SET 0
#define CONSTANT_BUFFER_BINDING 7
#define LINEAR_SAMPLER_SET 0
#define LINEAR_SAMPLER_BINDING 8
#define DIRECTIONAL_LIGHTS_SET 0
#define DIRECTIONAL_LIGHTS_BINDING 9
#define DIRECTIONAL_LIGHTS_SHADOWED_SET 0
#define DIRECTIONAL_LIGHTS_SHADOWED_BINDING 10
#define PUNCTUAL_LIGHTS_SET 0
#define PUNCTUAL_LIGHTS_BINDING 11
#define PUNCTUAL_LIGHTS_Z_BINS_SET 0
#define PUNCTUAL_LIGHTS_Z_BINS_BINDING 12
#define PUNCTUAL_LIGHTS_BIT_MASK_SET 0
#define PUNCTUAL_LIGHTS_BIT_MASK_BINDING 13
#define PUNCTUAL_LIGHTS_SHADOWED_SET 0
#define PUNCTUAL_LIGHTS_SHADOWED_BINDING 14
#define PUNCTUAL_LIGHTS_SHADOWED_Z_BINS_SET 0
#define PUNCTUAL_LIGHTS_SHADOWED_Z_BINS_BINDING 15
#define PUNCTUAL_LIGHTS_SHADOWED_BIT_MASK_SET 0
#define PUNCTUAL_LIGHTS_SHADOWED_BIT_MASK_BINDING 16
#define SHADOW_ATLAS_IMAGE_SET 0
#define SHADOW_ATLAS_IMAGE_BINDING 17

struct PushConsts
{
	float3 frustumCornerTL;
	float jitter;
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

struct Constants
{
	float4x4 viewMatrix;
	float4x4 prevViewMatrix;
	float4x4 prevProjMatrix;
	float4 reprojectedTexCoordScaleBias;
	float3 frustumCornerTL;
	float jitterX;
	float3 frustumCornerTR;
	float jitterY;
	float3 frustumCornerBL;
	float jitterZ;
	float3 frustumCornerBR;
	uint directionalLightCount;
	float3 cameraPos;
	uint directionalLightShadowedCount;
	uint punctualLightCount;
	uint punctualLightShadowedCount;
	
};
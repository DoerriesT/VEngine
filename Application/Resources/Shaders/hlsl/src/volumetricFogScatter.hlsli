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
#define CONSTANT_BUFFER_SET 0
#define CONSTANT_BUFFER_BINDING 6
#define DIRECTIONAL_LIGHTS_SET 0
#define DIRECTIONAL_LIGHTS_BINDING 7
#define DIRECTIONAL_LIGHTS_SHADOWED_SET 0
#define DIRECTIONAL_LIGHTS_SHADOWED_BINDING 8
#define PUNCTUAL_LIGHTS_SET 0
#define PUNCTUAL_LIGHTS_BINDING 9
#define PUNCTUAL_LIGHTS_Z_BINS_SET 0
#define PUNCTUAL_LIGHTS_Z_BINS_BINDING 10
#define PUNCTUAL_LIGHTS_BIT_MASK_SET 0
#define PUNCTUAL_LIGHTS_BIT_MASK_BINDING 11
#define PUNCTUAL_LIGHTS_SHADOWED_SET 0
#define PUNCTUAL_LIGHTS_SHADOWED_BINDING 12
#define PUNCTUAL_LIGHTS_SHADOWED_Z_BINS_SET 0
#define PUNCTUAL_LIGHTS_SHADOWED_Z_BINS_BINDING 13
#define PUNCTUAL_LIGHTS_SHADOWED_BIT_MASK_SET 0
#define PUNCTUAL_LIGHTS_SHADOWED_BIT_MASK_BINDING 14
#define SHADOW_ATLAS_IMAGE_SET 0
#define SHADOW_ATLAS_IMAGE_BINDING 15

struct Constants
{
	float4x4 viewMatrix;
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
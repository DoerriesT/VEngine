#define RESULT_IMAGE_BINDING 0
#define GLOBAL_MEDIA_BINDING 1
#define LOCAL_MEDIA_BINDING 2
#define LOCAL_MEDIA_Z_BINS_BINDING 3
#define LOCAL_MEDIA_BIT_MASK_BINDING 4
#define CONSTANT_BUFFER_BINDING 5
#define LINEAR_SAMPLER_BINDING 6
#define SHADOW_IMAGE_BINDING 7
#define SHADOW_SAMPLER_BINDING 8
#define SHADOW_MATRICES_BINDING 9
#define DIRECTIONAL_LIGHTS_BINDING 10
#define DIRECTIONAL_LIGHTS_SHADOWED_BINDING 11
#define PUNCTUAL_LIGHTS_BINDING 12
#define PUNCTUAL_LIGHTS_Z_BINS_BINDING 13
#define PUNCTUAL_LIGHTS_BIT_MASK_BINDING 14
#define PUNCTUAL_LIGHTS_SHADOWED_BINDING 15
#define PUNCTUAL_LIGHTS_SHADOWED_Z_BINS_BINDING 16
#define PUNCTUAL_LIGHTS_SHADOWED_BIT_MASK_BINDING 17
#define SHADOW_ATLAS_IMAGE_BINDING 18
#define EXPOSURE_DATA_BUFFER_BINDING 19
#define EXTINCTION_IMAGE_BINDING 20
#define FOM_IMAGE_BINDING 21

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
	float3 jitter1;
	uint punctualLightCount;
	uint punctualLightShadowedCount;
	uint useDithering;
	uint sampleCount;
	float extinctionVolumeTexelSize;
	float3 coordBias;
	float coordScale;
	int volumetricShadow;
	uint globalMediaCount;
	uint localMediaCount;
};
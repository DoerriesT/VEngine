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
#define POINT_LIGHT_Z_BINS_BUFFER_SET 0
#define POINT_LIGHT_Z_BINS_BUFFER_BINDING 9
#define POINT_LIGHT_BIT_MASK_BUFFER_SET 0
#define POINT_LIGHT_BIT_MASK_BUFFER_BINDING 10
#define POINT_LIGHT_DATA_SET 0
#define POINT_LIGHT_DATA_BINDING 11

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
	float3 frustumCornerTL;
	float jitterX;
	float3 frustumCornerTR;
	float jitterY;
	float3 frustumCornerBL;
	float jitterZ;
	float3 frustumCornerBR;
	int cascadeOffset;
	float3 cameraPos;
	int cascadeCount;
	float3 sunDirection;
	uint pointLightCount;
	float3 sunLightRadiance;
	uint padding4;
	float4 reprojectedTexCoordScaleBias;
};
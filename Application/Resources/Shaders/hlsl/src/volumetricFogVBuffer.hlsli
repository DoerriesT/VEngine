#define SCATTERING_EXTINCTION_IMAGE_SET 0
#define SCATTERING_EXTINCTION_IMAGE_BINDING 0
#define EMISSIVE_PHASE_IMAGE_SET 0
#define EMISSIVE_PHASE_IMAGE_BINDING 1

struct PushConsts
{
	float4 scatteringExtinction;
	float4 emissivePhase;
	float3 frustumCornerTL;
	float jitterX;
	float3 frustumCornerTR;
	float jitterY;
	float3 frustumCornerBL;
	float jitterZ;
	float3 frustumCornerBR;
	float padding0;
	float3 cameraPos;
};
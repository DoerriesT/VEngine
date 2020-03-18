#define SCATTERING_EXTINCTION_IMAGE_SET 0
#define SCATTERING_EXTINCTION_IMAGE_BINDING 0
#define EMISSIVE_PHASE_IMAGE_SET 0
#define EMISSIVE_PHASE_IMAGE_BINDING 1

struct PushConsts
{
	float4 scatteringExtinction;
	float4 emissivePhase;
};
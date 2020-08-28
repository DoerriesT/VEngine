#define PARTICLES_BINDING 0
#define CONSTANT_BUFFER_BINDING 1
#define DIRECTIONAL_LIGHTS_BINDING 2
#define DIRECTIONAL_LIGHTS_SHADOWED_BINDING 3
#define PUNCTUAL_LIGHTS_BINDING 4
#define PUNCTUAL_LIGHTS_Z_BINS_BINDING 5
#define PUNCTUAL_LIGHTS_BIT_MASK_BINDING 6
#define PUNCTUAL_LIGHTS_SHADOWED_BINDING 7
#define PUNCTUAL_LIGHTS_SHADOWED_Z_BINS_BINDING 8
#define PUNCTUAL_LIGHTS_SHADOWED_BIT_MASK_BINDING 9
#define VOLUMETRIC_FOG_IMAGE_BINDING 10
#define EXPOSURE_DATA_BUFFER_BINDING 11
#define SHADOW_ATLAS_IMAGE_BINDING 12
#define FOM_IMAGE_BINDING 14
#define SHADOW_MATRICES_BINDING 15
#define SHADOW_IMAGE_BINDING 16
#define BLUE_NOISE_IMAGE_BINDING 17
#define FOM_DIRECTIONAL_IMAGE_BINDING 18
#define FOM_DIRECTIONAL_DEPTH_RANGE_IMAGE_BINDING 19

struct Constants
{
	float4x4 viewProjectionMatrix;
	float4 viewMatrixDepthRow;
	float3 cameraPosition;
	uint width;
	float3 cameraUp;
	uint volumetricShadow;
	uint directionalLightCount;
	uint directionalLightShadowedCount;
	uint punctualLightCount;
	uint punctualLightShadowedCount;
	uint frame;
	uint height;
};
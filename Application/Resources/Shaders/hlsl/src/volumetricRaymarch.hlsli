#define RESULT_IMAGE_BINDING 0
#define GLOBAL_MEDIA_BINDING 1
#define CONSTANT_BUFFER_BINDING 2
#define SHADOW_IMAGE_BINDING 4
#define SHADOW_MATRICES_BINDING 5
#define DIRECTIONAL_LIGHTS_BINDING 6
#define DIRECTIONAL_LIGHTS_SHADOWED_BINDING 7
#define EXPOSURE_DATA_BUFFER_BINDING 8
#define DEPTH_IMAGE_BINDING 9
#define BLUE_NOISE_IMAGE_BINDING 10

struct Constants
{
	float4x4 viewMatrix;
	float4 unprojectParams;
	float3 frustumCornerTL;
	uint width;
	float3 frustumCornerTR;
	uint height;
	float3 frustumCornerBL;
	float rayOriginFactor;
	float3 frustumCornerBR;
	float rayOriginDist;
	float3 cameraPos;
	uint globalMediaCount;
	float2 texelSize;
	uint directionalLightCount;
	uint directionalLightShadowedCount;
	uint frame;
};
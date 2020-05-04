#define RESULT_IMAGE_SET 0
#define RESULT_IMAGE_BINDING 0
#define DEPTH_IMAGE_SET 0
#define DEPTH_IMAGE_BINDING 1
#define ALBEDO_ROUGHNESS_IMAGE_SET 0
#define ALBEDO_ROUGHNESS_IMAGE_BINDING 2
#define NORMAL_IMAGE_SET 0
#define NORMAL_IMAGE_BINDING 3
#define CONSTANT_BUFFER_SET 0
#define CONSTANT_BUFFER_BINDING 4
#define DIRECTIONAL_LIGHTS_SET 0
#define DIRECTIONAL_LIGHTS_BINDING 5
#define DIRECTIONAL_LIGHTS_SHADOWED_SET 0
#define DIRECTIONAL_LIGHTS_SHADOWED_BINDING 6


struct Constants
{
	float4x4 probeFaceToViewSpace[6];
	float4x4 viewMatrix;
	float2 texelSize;
	uint width;
	uint height;
	uint directionalLightCount;
	uint directionalLightShadowedCount;
};
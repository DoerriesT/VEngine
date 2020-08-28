#define RESULT_IMAGE_BINDING 0
#define DEPTH_IMAGE_BINDING 1
#define ALBEDO_ROUGHNESS_IMAGE_BINDING 2
#define NORMAL_IMAGE_BINDING 3
#define CONSTANT_BUFFER_BINDING 4
#define DIRECTIONAL_LIGHTS_BINDING 5
#define DIRECTIONAL_LIGHTS_SHADOWED_BINDING 6
#define DIRECTIONAL_LIGHTS_SHADOW_IMAGE_BINDING 7
#define SHADOW_MATRICES_BINDING 8


struct Constants
{
	float4x4 probeFaceToWorldSpace[6];
	float texelSize;
	uint width;
	uint directionalLightCount;
	uint directionalLightShadowedCount;
	uint arrayTextureOffset;
};
#define RAY_HIT_PDF_IMAGE_SET 0
#define RAY_HIT_PDF_IMAGE_BINDING 0
#define MASK_IMAGE_SET 0
#define MASK_IMAGE_BINDING 1
#define HIZ_PYRAMID_IMAGE_SET 0
#define HIZ_PYRAMID_IMAGE_BINDING 2
#define NORMAL_IMAGE_SET 0
#define NORMAL_IMAGE_BINDING 3
#define POINT_SAMPLER_SET 0
#define POINT_SAMPLER_BINDING 4

#define TEXTURES_SET 1
#define TEXTURES_BINDING 0
#define SAMPLERS_SET 1
#define SAMPLERS_BINDING 1


#define WIDTH_CONST_ID 0
#define HEIGHT_CONST_ID 1
#define TEXEL_WIDTH_CONST_ID 2
#define TEXEL_HEIGHT_CONST_ID 3

struct PushConsts
{
	mat4 projectionMatrix;
	vec4 unprojectParams;
	vec2 noiseScale;
	vec2 noiseJitter;
	float hiZMaxLevel;
	uint noiseTexId;
	float bias;
};
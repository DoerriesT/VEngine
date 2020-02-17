#define RESULT_IMAGE_SET 0
#define RESULT_IMAGE_BINDING 0
#define RAY_HIT_PDF_IMAGE_SET 0
#define RAY_HIT_PDF_IMAGE_BINDING 1
#define MASK_IMAGE_SET 0
#define MASK_IMAGE_BINDING 2
#define DEPTH_IMAGE_SET 0
#define DEPTH_IMAGE_BINDING 3
#define NORMAL_IMAGE_SET 0
#define NORMAL_IMAGE_BINDING 4
#define PREV_COLOR_IMAGE_SET 0
#define PREV_COLOR_IMAGE_BINDING 5
#define VELOCITY_IMAGE_SET 0
#define VELOCITY_IMAGE_BINDING 6
#define ALBEDO_IMAGE_SET 0
#define ALBEDO_IMAGE_BINDING 7
#define RESULT_MASK_IMAGE_SET 0
#define RESULT_MASK_IMAGE_BINDING 8

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
	vec4 unprojectParams;
	vec2 depthProjectParams;
	vec2 noiseScale;
	vec2 noiseJitter;
	uint noiseTexId;
	float diameterToScreen;
	float bias;
};
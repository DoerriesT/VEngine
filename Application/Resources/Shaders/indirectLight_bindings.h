#define RESULT_IMAGE_SET 0
#define RESULT_IMAGE_BINDING 0
#define DEPTH_IMAGE_SET 0
#define DEPTH_IMAGE_BINDING 1
#define ALBEDO_IMAGE_SET 0
#define ALBEDO_IMAGE_BINDING 2
#define NORMAL_IMAGE_SET 0
#define NORMAL_IMAGE_BINDING 3
#define INDIRECT_DIFFUSE_IMAGE_SET 0
#define INDIRECT_DIFFUSE_IMAGE_BINDING 4
#define INDIRECT_SPECULAR_IMAGE_SET 0
#define INDIRECT_SPECULAR_IMAGE_BINDING 5


#define WIDTH_CONST_ID 0
#define HEIGHT_CONST_ID 1
#define TEXEL_WIDTH_CONST_ID 2
#define TEXEL_HEIGHT_CONST_ID 3

struct PushConsts
{
	vec4 unprojectParams;
};
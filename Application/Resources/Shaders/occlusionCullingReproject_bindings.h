#define DEPTH_IMAGE_SET 0
#define DEPTH_IMAGE_BINDING 0
#define RESULT_IMAGE_SET 0
#define RESULT_IMAGE_BINDING 1

#define WIDTH_CONST_ID 0
#define HEIGHT_CONST_ID 1
#define TEXEL_WIDTH_CONST_ID 2
#define TEXEL_HEIGHT_CONST_ID 3

struct PushConsts
{
	mat4 prevToCurProjection;
};
#define DEPTH_IMAGE_SET 0
#define DEPTH_IMAGE_BINDING 0
#define TANGENT_SPACE_IMAGE_SET 0
#define TANGENT_SPACE_IMAGE_BINDING 1
#define RESULT_IMAGE_SET 0
#define RESULT_IMAGE_BINDING 2

struct PushConsts
{
	mat4 invProjection;
	vec4 resolution;
	float focalLength;
	float radius;
	float maxRadiusPixels;
	float numSteps;
	uint frame;
};
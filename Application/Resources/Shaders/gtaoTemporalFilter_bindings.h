#define INPUT_IMAGE_SET 0
#define INPUT_IMAGE_BINDING 0
#define RESULT_IMAGE_SET 0
#define RESULT_IMAGE_BINDING 1
#define VELOCITY_IMAGE_SET 0
#define VELOCITY_IMAGE_BINDING 2
#define PREVIOUS_IMAGE_SET 0
#define PREVIOUS_IMAGE_BINDING 3

struct PushConsts
{
	mat4 invViewProjectionNearFar;
	mat4 prevInvViewProjectionNearFar;
};
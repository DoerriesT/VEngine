#define RESULT_IMAGE_BINDING 0
#define INPUT_IMAGE_BINDING 1
#define HISTORY_IMAGE_BINDING 2
#define DEPTH_IMAGE_BINDING 3
#define VELOCITY_IMAGE_BINDING 4
#define EXPOSURE_DATA_BUFFER_BINDING 5

struct PushConsts
{
	uint width;
	uint height;
	float bicubicSharpness;
	float jitterOffsetWeight;
	uint ignoreHistory;
};
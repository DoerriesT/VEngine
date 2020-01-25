#define RESULT_IMAGE_SET 0
#define RESULT_IMAGE_BINDING 0
#define DEPTH_IMAGE_SET 0
#define DEPTH_IMAGE_BINDING 1
#define VELOCITY_IMAGE_SET 0
#define VELOCITY_IMAGE_BINDING 2
#define HISTORY_IMAGE_SET 0
#define HISTORY_IMAGE_BINDING 3
#define SOURCE_IMAGE_SET 0
#define SOURCE_IMAGE_BINDING 4

struct PushConsts
{
	float bicubicSharpness;
	float temporalContrastThreshold;
	float lowStrengthAlpha;
	float highStrengthAlpha;
	float antiFlickeringAlpha;
	float jitterOffsetWeight;
};
#define INPUT_IMAGE_SET 0
#define INPUT_IMAGE_BINDING 0
#define RESULT_IMAGE_SET 0
#define RESULT_IMAGE_BINDING 1

struct PushConsts
{
	vec2 invScreenSizeInPixels;
	float fxaaQualitySubpix;
	float fxaaQualityEdgeThreshold;
	float fxaaQualityEdgeThresholdMin;
};
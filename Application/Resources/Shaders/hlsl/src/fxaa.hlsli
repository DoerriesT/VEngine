#define INPUT_IMAGE_BINDING 0
#define RESULT_IMAGE_BINDING 1

struct PushConsts
{
	float2 invScreenSizeInPixels;
	float fxaaQualitySubpix;
	float fxaaQualityEdgeThreshold;
	float fxaaQualityEdgeThresholdMin;
	float time;
	uint applyDither;
};
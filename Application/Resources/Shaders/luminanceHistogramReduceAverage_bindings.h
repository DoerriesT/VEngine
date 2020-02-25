#include "common.h"

#define LUMINANCE_HISTOGRAM_SET 0
#define LUMINANCE_HISTOGRAM_BINDING 0
#define LUMINANCE_VALUES_SET 0
#define LUMINANCE_VALUES_BINDING 1

struct PushConsts
{
	float precomputedTermUp;
	float precomputedTermDown;
	float invScale;
	float bias;
	uint lowerBound;
	uint upperBound;
	uint currentResourceIndex;
	uint previousResourceIndex;
};
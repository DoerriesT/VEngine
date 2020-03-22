#include "common.h"

#define POINT_LIGHT_MASK_SET 0
#define POINT_LIGHT_MASK_BINDING 0
#define SPOT_LIGHT_MASK_SET 0
#define SPOT_LIGHT_MASK_BINDING 1

struct PushConsts
{
	mat4 transform;
	uint index;
	uint alignedDomainSizeX;
	uint wordCount;
	uint targetBuffer;
};
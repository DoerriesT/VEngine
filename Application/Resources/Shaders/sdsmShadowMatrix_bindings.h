#include "common.h"

#define PARTITION_BOUNDS_SET 0
#define PARTITION_BOUNDS_BINDING 0
#define SHADOW_DATA_SET 0
#define SHADOW_DATA_BINDING 1

struct PushConsts
{
	mat4 cameraViewToLightView;
	mat4 lightView;
	// float lightSpaceNear;	-> lightView[3][1]
	// float lightSpaceFar;		-> lightView[3][2]
};
#include "common.h"

#define DEPTH_BOUNDS_SET 0
#define DEPTH_BOUNDS_BINDING 0
#define SHADOW_DATA_SET 0
#define SHADOW_DATA_BINDING 1

struct PushConsts
{
	mat4 cameraViewToLightView;
	mat4 lightView;
	// float nearPlane;			-> lightView[0][3]
	// float farPlane;			-> lightView[1][3]
	// float projScaleXInv;		-> lightView[2][3]
	// float projScaleYInv;		-> lightView[3][0]
	// float lightSpaceNear;	-> lightView[3][1]
	// float lightSpaceFar;		-> lightView[3][2]
};
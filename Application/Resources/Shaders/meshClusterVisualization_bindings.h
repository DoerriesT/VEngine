#include "common.h"

#define INSTANCE_DATA_SET 0
#define INSTANCE_DATA_BINDING 0
#define TRANSFORM_DATA_SET 0
#define TRANSFORM_DATA_BINDING 1

struct PushConsts
{
	mat4 jitteredViewProjectionMatrix;
};
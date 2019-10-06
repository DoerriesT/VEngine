#include "common.h"

#define INSTANCE_DATA_SET 0
#define INSTANCE_DATA_BINDING 0
#define TRANSFORM_DATA_SET 0
#define TRANSFORM_DATA_BINDING 1
#define AABB_DATA_SET 0
#define AABB_DATA_BINDING 2
#define VISIBILITY_BUFFER_SET 0
#define VISIBILITY_BUFFER_BINDING 3


struct PushConsts
{
	mat4 jitteredViewProjectionMatrix;
};
#include "common.h"

#define TEXTURES_SET 0
#define TEXTURES_BINDING 0

struct PushConsts
{
	vec4 scaleBias;
	vec4 color;
	vec2 texCoordOffset;
	vec2 texCoordSize;
	uint textureIndex;
};
#version 450

#include "shadows_bindings.h"

#ifndef ALPHA_MASK_ENABLED
#define ALPHA_MASK_ENABLED 0
#endif // ALPHA_MASK_ENABLED

layout(set = TRANSFORM_DATA_SET, binding = TRANSFORM_DATA_BINDING) readonly buffer TRANSFORM_DATA 
{
    mat4 uTransformData[];
};

layout(push_constant) uniform PUSH_CONSTS 
{
	PushConsts uPushConsts;
};

layout(location = 0) in vec3 inPosition;
#if ALPHA_MASK_ENABLED
layout(location = 2) in vec2 inTexCoord;


layout(location = 0) out vec2 vTexCoord;
#endif //ALPHA_MASK_ENABLED

void main() 
{
    gl_Position = uPushConsts.viewProjectionMatrix * uTransformData[uPushConsts.transformIndex] * vec4(inPosition, 1.0);
	
	#if ALPHA_MASK_ENABLED
	vTexCoord = inTexCoord;
	#endif //ALPHA_MASK_ENABLED
}


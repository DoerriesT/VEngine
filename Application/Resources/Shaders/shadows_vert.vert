#version 450

#include "shadows_bindings.h"

layout(set = TRANSFORM_DATA_SET, binding = TRANSFORM_DATA_BINDING) readonly buffer TRANSFORM_DATA 
{
    mat4 uTransformData[];
};

layout(push_constant) uniform PUSH_CONSTS 
{
	PushConsts uPushConsts;
};

layout(location = 0) in vec3 inPosition;

void main() 
{
    gl_Position = uPushConsts.viewProjectionMatrix * uTransformData[uPushConsts.transformIndex] * vec4(inPosition, 1.0);
}


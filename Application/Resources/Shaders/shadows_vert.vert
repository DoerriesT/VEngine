#version 450

#include "common.h"

layout(push_constant) uniform PushConsts 
{
	mat4 viewProjectionMatrix;
	uint transformIndex;
} uPushConsts;

layout(location = 0) in vec3 inPosition;

void main() 
{
    gl_Position = uPushConsts.viewProjectionMatrix * uTransformData.data[uPushConsts.transformIndex] * vec4(inPosition, 1.0);
}


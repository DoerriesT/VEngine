#version 450

#include "rasterTiling_bindings.h"

layout(location = 0) in vec3 inPosition;

layout(push_constant) uniform PUSH_CONSTS 
{
	PushConsts uPushConsts;
};

void main() 
{
    gl_Position = uPushConsts.transform * vec4(inPosition, 1.0);
}


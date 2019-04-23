#version 450

#include "geometry_bindings.h"

layout(set = CONSTANT_DATA_SET, binding = CONSTANT_DATA_BINDING) uniform CONSTANT_DATA
{
	ConstantData uConstantData;
};

layout(set = TRANSFORM_DATA_SET, binding = TRANSFORM_DATA_BINDING) readonly buffer TRANSFORM_DATA 
{
    mat4 uTransformData[];
};

layout(push_constant) uniform PUSH_CONSTS 
{
	PushConsts uPushConsts;
};

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec2 vTexCoord;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec3 vWorldPos;

void main() 
{
	mat4 modelMatrix = uTransformData[uPushConsts.transformIndex];
    gl_Position = uConstantData.jitteredViewProjectionMatrix * modelMatrix * vec4(inPosition, 1.0);
	vTexCoord = inTexCoord;
	vNormal = mat3(uConstantData.viewMatrix * modelMatrix) * inNormal;
	vWorldPos = (uConstantData.viewMatrix * modelMatrix * vec4(inPosition, 1.0)).xyz;
}


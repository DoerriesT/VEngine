#version 450

#include "common.h"

layout(push_constant) uniform PushConsts 
{
	uint transformIndex;
	uint materialIndex;
} uPushConsts;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec2 vTexCoord;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec3 vWorldPos;

void main() 
{
	mat4 modelMatrix = uTransformData.data[uPushConsts.transformIndex];
    gl_Position = uPerFrameData.jitteredViewProjectionMatrix * modelMatrix * vec4(inPosition, 1.0);
	vTexCoord = inTexCoord;
	vNormal = mat3(uPerFrameData.viewMatrix * modelMatrix) * inNormal;
	vWorldPos = (uPerFrameData.viewMatrix * modelMatrix * vec4(inPosition, 1.0)).xyz;
}


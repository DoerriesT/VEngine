#version 450

#include "geometry_bindings.h"

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
layout(location = 3) flat out uint vMaterialIndex;

void main() 
{
	PushConsts pushConsts = uPushConsts;
	const mat4 modelMatrix = uTransformData[pushConsts.transformIndex];
    gl_Position = pushConsts.jitteredViewProjectionMatrix * modelMatrix * vec4(inPosition, 1.0);
	const mat4 viewMatrix = transpose(mat4(pushConsts.viewMatrixRow0, pushConsts.viewMatrixRow1, pushConsts.viewMatrixRow2, vec4(0.0, 0.0, 0.0, 1.0)));
	const mat4 modelViewMatrix = viewMatrix * modelMatrix;										
	vTexCoord = inTexCoord;
	vNormal = mat3(modelViewMatrix) * inNormal;
	vWorldPos = (modelViewMatrix * vec4(inPosition, 1.0)).xyz;
	vMaterialIndex= pushConsts.materialIndex;
}


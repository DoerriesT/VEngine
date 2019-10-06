#version 450

#include "meshClusterVisualization_bindings.h"

layout(set = INSTANCE_DATA_SET, binding = INSTANCE_DATA_BINDING) readonly buffer INSTANCE_DATA 
{
    SubMeshInstanceData uInstanceData[];
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

void main() 
{
	SubMeshInstanceData instanceData = uInstanceData[gl_InstanceIndex];
	const mat4 modelMatrix = uTransformData[instanceData.transformIndex];
    gl_Position = uPushConsts.jitteredViewProjectionMatrix * modelMatrix * vec4(inPosition, 1.0);
}


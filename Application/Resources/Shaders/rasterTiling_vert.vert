#version 450

layout(location = 0) in vec3 inPosition;

layout(location = 0) out flat uint vIndex;
layout(location = 1) out flat uint vAlignedDomainSizeX;

layout(push_constant) uniform PushConsts 
{
	mat4 transform;
	uint index;
	uint alignedDomainSizeX;
} uPushConsts;



void main() 
{
    gl_Position = uPushConsts.transform * vec4(inPosition, 1.0);
	vIndex = uPushConsts.index;
	vAlignedDomainSizeX = uPushConsts.alignedDomainSizeX;
}


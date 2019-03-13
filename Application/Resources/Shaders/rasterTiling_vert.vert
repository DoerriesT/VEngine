#version 450

layout(location = 0) in vec3 inPosition;

layout(location = 0) out flat uint vIndex;
layout(location = 1) out flat uint vAlignedDomainSizeX;

layout(set = 0, binding = 0) uniform PerFrameData 
{
	float time;
	float fovy;
	float nearPlane;
	float farPlane;
	mat4 viewMatrix;
	mat4 projectionMatrix;
	mat4 viewProjectionMatrix;
	mat4 invViewMatrix;
	mat4 invProjectionMatrix;
	mat4 invViewProjectionMatrix;
	mat4 prevViewMatrix;
	mat4 prevProjectionMatrix;
	mat4 prevViewProjectionMatrix;
	mat4 prevInvViewMatrix;
	mat4 prevInvProjectionMatrix;
	mat4 prevInvViewProjectionMatrix;
	vec4 cameraPosition;
	vec4 cameraDirection;
	uint frame;
} uPerFrameData;

layout(push_constant) uniform PushConsts 
{
	mat4 modelMatrix;
	uint index;
	uint alignedDomainSizeX;
} uPushConsts;



void main() 
{
    gl_Position = uPerFrameData.viewProjectionMatrix * uPushConsts.modelMatrix * vec4(inPosition, 1.0);
	vIndex = uPushConsts.index;
	vAlignedDomainSizeX = uPushConsts.alignedDomainSizeX;
}


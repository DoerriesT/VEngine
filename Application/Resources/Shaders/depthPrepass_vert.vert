#version 450
#extension GL_ARB_separate_shader_objects : enable

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

layout(set = 0, binding = 1) uniform PerDrawData 
{
    vec4 albedoFactorMetallic;
	vec4 emissiveFactorRoughness;
	mat4 modelMatrix;
	uint albedoTexture;
	uint normalTexture;
	uint metallicTexture;
	uint roughnessTexture;
	uint occlusionTexture;
	uint emissiveTexture;
	uint displacementTexture;
	uint padding;
} uPerDrawData;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec2 vTexCoord;

out gl_PerVertex 
{
    vec4 gl_Position;
};

void main() 
{
    gl_Position = uPerFrameData.viewProjectionMatrix * uPerDrawData.modelMatrix * vec4(inPosition, 1.0);
	vTexCoord = inTexCoord;
}


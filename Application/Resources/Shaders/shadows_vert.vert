#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform PushConsts 
{
	mat4 viewProjectionMatrix;
} uPushConsts;

layout(set = 0, binding = 0) uniform PerDrawData 
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

out gl_PerVertex 
{
    vec4 gl_Position;
};

void main() 
{
    gl_Position = uPushConsts.viewProjectionMatrix * uPerDrawData.modelMatrix * vec4(inPosition, 1.0);
}


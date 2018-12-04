#version 450
#extension GL_ARB_separate_shader_objects : enable

#ifndef PI
#define PI (3.14159265359)
#endif // PI

#ifndef TEXTURE_ARRAY_SIZE
#define TEXTURE_ARRAY_SIZE (512)
#endif // TEXTURE_ARRAY_SIZE


layout(location = 0) in vec2 vTexCoord;

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

layout(set = 1, binding = 0) uniform sampler2D uTextures[TEXTURE_ARRAY_SIZE];


const float ALPHA_CUTOFF = 0.9;
const float MIP_SCALE = 0.25;

void main() 
{
	if (uPerDrawData.albedoTexture != 0)
	{
		float alpha = texture(uTextures[uPerDrawData.albedoTexture - 1], vTexCoord).a;
		alpha *= 1.0 + textureQueryLod(uTextures[uPerDrawData.albedoTexture - 1], vTexCoord).x * MIP_SCALE;
		if(alpha < ALPHA_CUTOFF)
		{
			discard;
		}
	}
}


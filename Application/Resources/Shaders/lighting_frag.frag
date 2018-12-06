#version 450
#extension GL_ARB_separate_shader_objects : enable

#ifndef PI
#define PI (3.14159265359)
#endif // PI

layout(location = 0) in vec2 vTexCoord;

layout(location = 0) out vec4 oFragColor;

layout(set = 1, input_attachment_index = 0, binding = 0) uniform subpassInput uDepthTexture;
layout(set = 1, input_attachment_index = 1, binding = 1) uniform subpassInput uAlbedoTexture;
layout(set = 1, input_attachment_index = 2, binding = 2) uniform subpassInput uNormalTexture;
layout(set = 1, input_attachment_index = 3, binding = 3) uniform subpassInput uMetallicRoughnessOcclusionTexture;

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

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a2 = roughness*roughness;
    a2 *= a2;
    float NdotH2 = max(dot(N, H), 0.0);
    NdotH2 *= NdotH2;

    float nom   = a2;
    float denom = NdotH2 * (a2 - 1.0) + 1.0;

    denom = PI * denom * denom;

    return nom / max(denom, 0.0000001);
}

float GeometrySmith(float NdotV, float NdotL, float roughness)
{
	float r = (roughness + 1.0);
    float k = (r*r) / 8.0;
    float ggx2 =  NdotV / max(NdotV * (1.0 - k) + k, 0.0000001);
    float ggx1 = NdotL / max(NdotL * (1.0 - k) + k, 0.0000001);

    return ggx1 * ggx2;
}


vec3 fresnelSchlick(float HdotV, vec3 F0)
{
	float power = (-5.55473 * HdotV - 6.98316) * HdotV;
	return F0 + (1.0 - F0) * pow(2.0, power);
}

vec3 accurateLinearToSRGB(in vec3 linearCol)
{
	vec3 sRGBLo = linearCol * 12.92;
	vec3 sRGBHi = (pow(abs(linearCol), vec3(1.0/2.4)) * 1.055) - 0.055;
	vec3 sRGB = mix(sRGBLo, sRGBHi, vec3(greaterThan(linearCol, vec3(0.0031308))));
	return sRGB;
}

vec3 accurateSRGBToLinear(in vec3 sRGBCol)
{
	vec3 linearRGBLo = sRGBCol * (1.0 / 12.92);
	vec3 linearRGBHi = pow((sRGBCol + vec3(0.055)) * vec3(1.0 / 1.055), vec3(2.4));
	vec3 linearRGB = mix(linearRGBLo, linearRGBHi, vec3(greaterThan(sRGBCol, vec3(0.04045))));
	return linearRGB;
}

const vec3 lightDir = normalize(vec3(0.1, 3.0, -1.0));

void main() 
{
	vec3 albedo = accurateSRGBToLinear(subpassLoad(uAlbedoTexture).rgb);
	vec3 N = subpassLoad(uNormalTexture).xyz;
	float depth = subpassLoad(uDepthTexture).x;
	
	const vec4 clipSpacePosition = vec4(vec3(vTexCoord, depth) * 2.0 - 1.0, 1.0);
	vec4 viewSpacePosition = uPerFrameData.invProjectionMatrix * clipSpacePosition;
	viewSpacePosition /= viewSpacePosition.w;
	
	vec3 V = -normalize(viewSpacePosition.xyz);
	vec3 L = mat3(uPerFrameData.viewMatrix) * lightDir;
	vec3 H = normalize(V + L);
    
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	
	float metallic = subpassLoad(uMetallicRoughnessOcclusionTexture).x;
	float roughness = subpassLoad(uMetallicRoughnessOcclusionTexture).y;
	
	// Cook-Torrance BRDF
	float NDF = DistributionGGX(N, H, roughness);
	float G = GeometrySmith(NdotV, NdotL, roughness);
	
	vec3 F0 = mix(vec3(0.04), albedo, metallic);
	vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    
	vec3 nominator = NDF * G * F;
	float denominator = max(4 * NdotV * NdotL, 0.0000001);
    
	vec3 specular = nominator / denominator;
    
	// because of energy conversion kD and kS must add up to 1.0
	vec3 kD = vec3(1.0) - F;
	// multiply kD by the inverse metalness so if a material is metallic, it has no diffuse lighting (and otherwise a blend)
	kD *= 1.0 - metallic;
    
	oFragColor = vec4((kD * albedo / PI + specular) * vec3(5.0) * NdotL, 1.0);
	oFragColor.rgb = accurateLinearToSRGB(oFragColor.rgb);
}
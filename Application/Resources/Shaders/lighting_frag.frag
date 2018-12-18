#version 450
#extension GL_ARB_separate_shader_objects : enable

#ifndef PI
#define PI (3.14159265359)
#endif // PI

#ifndef TEXTURE_ARRAY_SIZE
#define TEXTURE_ARRAY_SIZE (512)
#endif // TEXTURE_ARRAY_SIZE

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

layout(push_constant) uniform PushConsts 
{
	uint directionalLightCount;
} uPushConsts;

struct ShadowData
{
	mat4 shadowViewProjectionMatrix;
	vec4 shadowCoordScaleBias;
};

struct DirectionalLight
{
	vec4 color;
	vec4 direction;
	uint shadowDataOffset;
	uint shadowDataCount;
};

struct PointLight
{
	vec4 positionRadius;
	vec4 colorInvSqrAttRadius;
	uint shadowDataOffset;
	uint shadowDataCount;
};

struct SpotLight
{
	vec4 colorInvSqrAttRadius;
	vec4 positionAngleScale;
	vec4 directionAngleOffset;
	vec4 boundingSphere;
	uint shadowDataOffset;
	uint shadowDataCount;
};


layout(std140, set = 2, binding = 0) readonly buffer DirectionalLights 
{
	DirectionalLight lights[];
} uDirectionalLights;

layout(std140, set = 2, binding = 1) readonly buffer PointLights 
{
	PointLight lights[];
} uPointLights;

layout(std140, set = 2, binding = 2) readonly buffer SpotLights 
{
	SpotLight lights[];
} uSpotLights;

layout(std140, set = 2, binding = 3) readonly buffer Shadows 
{
	ShadowData data[];
} uShadowData;

layout(set = 2, binding = 4) uniform sampler2DShadow uShadowTexture;

layout(std140, set = 3, binding = 0) readonly buffer PointLightIndices 
{
	uint count;
	uint indices[];
} uPointLightIndices;

layout(std140, set = 3, binding = 1) readonly buffer SpotLightIndices 
{
	uint count;
	uint indices[];
} uSpotLightIndices;

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

vec2 computeReceiverPlaneDepthBias(vec3 texCoordDX, vec3 texCoordDY)
{
    vec2 biasUV;
    biasUV.x = texCoordDY.y * texCoordDX.z - texCoordDX.y * texCoordDY.z;
    biasUV.y = texCoordDX.x * texCoordDY.z - texCoordDY.x * texCoordDX.z;
    biasUV *= 1.0 / ((texCoordDX.x * texCoordDY.y) - (texCoordDX.y * texCoordDY.x));
    return biasUV;
}

float sampleShadowTexture(in vec2 base_uv, in float u, in float v, in vec2 shadowMapSizeInv, in float depth, in vec2 receiverPlaneDepthBias) 
{
    vec2 uv = base_uv + vec2(u, v) * shadowMapSizeInv;

    float z = depth + dot(vec2(u, v) * shadowMapSizeInv, receiverPlaneDepthBias);

	return texture(uShadowTexture, vec3(uv, z)).x;
}

// based on https://mynameismjp.wordpress.com/2013/09/10/shadow-maps/
float shadowOptimizedPCF(vec3 shadowPos, vec3 shadowPosDx, vec3 shadowPosDy, vec2 shadowTextureSize, vec2 invShadowTextureSize)
{
    float lightDepth = shadowPos.z;
    vec2 receiverPlaneDepthBias = computeReceiverPlaneDepthBias(shadowPosDx, shadowPosDy);

    // Static depth biasing to make up for incorrect fractional sampling on the shadow map grid
    float fractionalSamplingError = 2.0 * dot(vec2(1.0, 1.0) * invShadowTextureSize, abs(receiverPlaneDepthBias));
    lightDepth -= min(fractionalSamplingError, 0.01);

    vec2 uv = shadowPos.xy * shadowTextureSize; // 1 unit - 1 texel
	
    vec2 base_uv;
    base_uv.x = floor(uv.x + 0.5);
    base_uv.y = floor(uv.y + 0.5);

    float s = (uv.x + 0.5 - base_uv.x);
    float t = (uv.y + 0.5 - base_uv.y);

    base_uv -= vec2(0.5, 0.5);
    base_uv *= invShadowTextureSize;

    float sum = 0;

    float uw0 = (5 * s - 6);
    float uw1 = (11 * s - 28);
    float uw2 = -(11 * s + 17);
    float uw3 = -(5 * s + 1);

    float u0 = (4 * s - 5) / uw0 - 3;
    float u1 = (4 * s - 16) / uw1 - 1;
    float u2 = -(7 * s + 5) / uw2 + 1;
    float u3 = -s / uw3 + 3;

    float vw0 = (5 * t - 6);
    float vw1 = (11 * t - 28);
    float vw2 = -(11 * t + 17);
    float vw3 = -(5 * t + 1);

    float v0 = (4 * t - 5) / vw0 - 3;
    float v1 = (4 * t - 16) / vw1 - 1;
    float v2 = -(7 * t + 5) / vw2 + 1;
    float v3 = -t / vw3 + 3;

    sum += uw0 * vw0 * sampleShadowTexture(base_uv, u0, v0, invShadowTextureSize, lightDepth, receiverPlaneDepthBias);
    sum += uw1 * vw0 * sampleShadowTexture(base_uv, u1, v0, invShadowTextureSize, lightDepth, receiverPlaneDepthBias);
    sum += uw2 * vw0 * sampleShadowTexture(base_uv, u2, v0, invShadowTextureSize, lightDepth, receiverPlaneDepthBias);
    sum += uw3 * vw0 * sampleShadowTexture(base_uv, u3, v0, invShadowTextureSize, lightDepth, receiverPlaneDepthBias);

    sum += uw0 * vw1 * sampleShadowTexture(base_uv, u0, v1, invShadowTextureSize, lightDepth, receiverPlaneDepthBias);
    sum += uw1 * vw1 * sampleShadowTexture(base_uv, u1, v1, invShadowTextureSize, lightDepth, receiverPlaneDepthBias);
    sum += uw2 * vw1 * sampleShadowTexture(base_uv, u2, v1, invShadowTextureSize, lightDepth, receiverPlaneDepthBias);
    sum += uw3 * vw1 * sampleShadowTexture(base_uv, u3, v1, invShadowTextureSize, lightDepth, receiverPlaneDepthBias);

    sum += uw0 * vw2 * sampleShadowTexture(base_uv, u0, v2, invShadowTextureSize, lightDepth, receiverPlaneDepthBias);
    sum += uw1 * vw2 * sampleShadowTexture(base_uv, u1, v2, invShadowTextureSize, lightDepth, receiverPlaneDepthBias);
    sum += uw2 * vw2 * sampleShadowTexture(base_uv, u2, v2, invShadowTextureSize, lightDepth, receiverPlaneDepthBias);
    sum += uw3 * vw2 * sampleShadowTexture(base_uv, u3, v2, invShadowTextureSize, lightDepth, receiverPlaneDepthBias);

    sum += uw0 * vw3 * sampleShadowTexture(base_uv, u0, v3, invShadowTextureSize, lightDepth, receiverPlaneDepthBias);
    sum += uw1 * vw3 * sampleShadowTexture(base_uv, u1, v3, invShadowTextureSize, lightDepth, receiverPlaneDepthBias);
    sum += uw2 * vw3 * sampleShadowTexture(base_uv, u2, v3, invShadowTextureSize, lightDepth, receiverPlaneDepthBias);
    sum += uw3 * vw3 * sampleShadowTexture(base_uv, u3, v3, invShadowTextureSize, lightDepth, receiverPlaneDepthBias);

    return sum * 1.0f / 2704;
}

vec3 evaluateDirectionalLight(
	inout mat3 viewMatrix,
	inout vec3 albedo, 
	inout vec3 N, 
	inout vec3 F0, 
	inout vec3 V,
	inout vec3 viewSpacePosition,
	inout float NdotV,
	inout float metallic, 
	inout float roughness,
	uint index)
{
	vec3 L = viewMatrix * uDirectionalLights.lights[index].direction.xyz;
	vec3 H = normalize(V + L);
    
	float NdotL = max(dot(N, L), 0.0);
	
	float NDF = DistributionGGX(N, H, roughness);
	float G = GeometrySmith(NdotV, NdotL, roughness);
	vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    
	vec3 numerator = NDF * G * F;
	float denominator = max(4 * NdotV * NdotL, 0.0000001);
    
	vec3 specular = numerator / denominator;
    
	// because of energy conversion kD and kS must add up to 1.0
	vec3 kD = vec3(1.0) - F;
	// multiply kD by the inverse metalness so if a material is metallic, it has no diffuse lighting (and otherwise a blend)
	kD *= 1.0 - metallic;
    
	vec3 result = (kD * albedo * (1.0 / PI) + specular) * NdotL * uDirectionalLights.lights[index].color.rgb;
	
	uint shadowDataCount = uDirectionalLights.lights[index].shadowDataCount;
	if(shadowDataCount > 0)
	{		
		uint shadowDataOffset = uDirectionalLights.lights[index].shadowDataOffset;
		vec3 shadowCoord = vec3(2.0);
		vec2 invShadowTextureSize = 1.0 / textureSize(uShadowTexture, 0).xy;
		vec4 offsetPos = uPerFrameData.invViewMatrix * vec4(invShadowTextureSize.x * clamp(1.0 - NdotL, 0.0, 1.0) * N + viewSpacePosition, 1.0);

		for (uint i = 0; i < shadowDataCount; ++i)
		{
			const vec4 projCoords4 = 
			uShadowData.data[shadowDataOffset + i].shadowViewProjectionMatrix * offsetPos;
			shadowCoord = (projCoords4.xyz / projCoords4.w);
			shadowCoord.xy = shadowCoord.xy * 0.5 + 0.5; 
			
			// test if projected coordinate is inside texture
			// add small guard band at edges to avoid PCF sampling outside texture
			if(all(greaterThanEqual(shadowCoord.xy, vec2(0.003))) && all(lessThan(shadowCoord.xy, vec2(1.0 - 0.003))))
			{
				vec4 scaleBias = uShadowData.data[shadowDataOffset + i].shadowCoordScaleBias;
				shadowCoord.xy = shadowCoord.xy * scaleBias.xy + scaleBias.zw;
				break;
			}
		}

		
		float shadow = shadowOptimizedPCF(shadowCoord, dFdxFine(shadowCoord), dFdyFine(shadowCoord), textureSize(uShadowTexture, 0).xy, invShadowTextureSize);
		
		result *= (1.0 - shadow);
	}
	return result;
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

void main() 
{
	float depth = subpassLoad(uDepthTexture).x;
	
	if (depth == 1.0)
	{
		oFragColor = vec4(vec3(0.529, 0.808, 0.922), 1.0);
		return;
	}
	
	const vec4 clipSpacePosition = vec4(vec2(vTexCoord) * 2.0 - 1.0, depth, 1.0);
	vec4 viewSpacePosition = uPerFrameData.invProjectionMatrix * clipSpacePosition;
	viewSpacePosition /= viewSpacePosition.w;
	
	mat3 viewMatrix = mat3(uPerFrameData.viewMatrix);
	vec3 albedo = accurateSRGBToLinear(subpassLoad(uAlbedoTexture).rgb);
	vec3 N = subpassLoad(uNormalTexture).xyz;
	float metallic = subpassLoad(uMetallicRoughnessOcclusionTexture).x;
	vec3 F0 = mix(vec3(0.04), albedo, metallic);
	vec3 V = -normalize(viewSpacePosition.xyz);
	float NdotV = max(dot(N, V), 0.0);
	float roughness = subpassLoad(uMetallicRoughnessOcclusionTexture).y;
	
	oFragColor = vec4(0.0, 0.0, 0.0, 1.0);
	
	for (uint i = 0; i < uPushConsts.directionalLightCount; ++i)
	{
		oFragColor.rgb += evaluateDirectionalLight(viewMatrix, albedo, N, F0, V, viewSpacePosition.xyz, NdotV, metallic, roughness, i);
	}
	oFragColor.rgb += 0.1 * albedo;
	
	oFragColor.rgb = accurateLinearToSRGB(oFragColor.rgb);
}
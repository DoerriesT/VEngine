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

float interleavedGradientNoise(vec2 v)
{
	vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
	return fract(magic.z * dot(v, magic.xy));
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
		vec4 offsetPos = uPerFrameData.invViewMatrix * vec4(0.1 * L + viewSpacePosition, 1.0);

		uint split = 0;
		for (; split < shadowDataCount; ++split)
		{
			const vec4 projCoords4 = 
			uShadowData.data[shadowDataOffset + split].shadowViewProjectionMatrix * offsetPos;
			shadowCoord = (projCoords4.xyz / projCoords4.w);
			shadowCoord.xy = shadowCoord.xy * 0.5 + 0.5; 
			
			// test if projected coordinate is inside texture
			// add small guard band at edges to avoid PCF sampling outside texture
			if(all(greaterThanEqual(shadowCoord.xy, vec2(0.003))) && all(lessThan(shadowCoord.xy, vec2(1.0 - 0.003))))
			{
				vec4 scaleBias = uShadowData.data[shadowDataOffset + split].shadowCoordScaleBias;
				shadowCoord.xy = shadowCoord.xy * scaleBias.xy + scaleBias.zw;
				break;
			}
		}

		
		float shadow = 0.0;
		
		const float noise = interleavedGradientNoise(gl_FragCoord.xy);// * 0.5 + (uOddFrame ? 0.5 : 0.0);

		const float rotSin = sin(2.0 * PI * noise);
		const float rotCos = cos(2.0 * PI * noise);

		const mat2 rotation = mat2(rotCos, rotSin, -rotSin, rotCos);

		const vec2 samples[8] = 
		{ 
			vec2(-0.7071, 0.7071),
			vec2(0.0, -0.8750),
			vec2(0.5303, 0.5303),
			vec2(-0.625, 0.0),
			vec2(0.3536, -0.3536),
			vec2(0.0, 0.375),
			vec2(-0.1768, -0.1768),
			vec2(0.125, 0.0)
		};

		const float splitMult = split == 0 ? 5.0 : split == 1 ? 2.0 : 1.0;

		for(int i = 0; i < 8; ++i)
		{
			vec2 offset = rotation * samples[i];
			shadow += texture(uShadowTexture, vec3(shadowCoord.xy + offset * invShadowTextureSize * splitMult, shadowCoord.z)).x;
		}
		shadow *= 1.0 / 8.0;
		
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
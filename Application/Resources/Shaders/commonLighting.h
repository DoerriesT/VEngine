#ifndef DIFFUSE_ONLY
#define DIFFUSE_ONLY 0
#endif // DIFFUSE_ONLY

#ifndef SHADOW_FUNCTIONS
#define SHADOW_FUNCTIONS 1
#endif // SHADOW_FUNCTIONS

struct LightingParams
{
	vec3 albedo;
	vec3 N;
	vec3 V;
	vec3 viewSpacePosition;
	float metalness;
	float roughness;
};

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

float smoothDistanceAtt(float squaredDistance, float invSqrAttRadius)
{
	float factor = squaredDistance * invSqrAttRadius;
	float smoothFactor = clamp(1.0 - factor * factor, 0.0, 1.0);
	return smoothFactor * smoothFactor;
}

float getDistanceAtt(vec3 unnormalizedLightVector, float invSqrAttRadius)
{
	float sqrDist = dot(unnormalizedLightVector, unnormalizedLightVector);
	float attenuation = 1.0 / (max(sqrDist, invSqrAttRadius));
	attenuation *= smoothDistanceAtt(sqrDist, invSqrAttRadius);
	
	return attenuation;
}

vec3 diffuseBrdf(const LightingParams params, vec3 radiance, vec3 L)
{
	const float NdotL = max(dot(params.N, L), 0.0);
	return params.albedo * (1.0 / PI) * NdotL * radiance;
}

vec3 cookTorranceSpecularBrdf(const LightingParams params, vec3 radiance, vec3 L)
{
	const vec3 H = normalize(params.V + L);
	const float NdotL = max(dot(params.N, L), 0.0);
	const float NdotV = max(dot(params.N, params.V), 0.0);
	
	// Cook-Torrance BRDF
	const float NDF = DistributionGGX(params.N, H, params.roughness);
	const float G = GeometrySmith(NdotV, NdotL, params.roughness);
	const vec3 F0 = mix(vec3(0.04), params.albedo, params.metalness);
	const vec3 F = fresnelSchlick(max(dot(H, params.V), 0.0), F0);
	
	const vec3 numerator = NDF * G * F;
	const float denominator = max(4.0 * NdotV * NdotL, 1e-6);

	const vec3 specular = numerator * (1.0 / denominator);
	
	// because of energy conversion kD and kS must add up to 1.0.
	// multiply kD by the inverse metalness so if a material is metallic, it has no diffuse lighting (and otherwise a blend)
	const vec3 kD = (vec3(1.0) - F) * (1.0 - params.metalness);

	return (kD * params.albedo * (1.0 / PI) + specular) * radiance * NdotL;
}

vec3 evaluatePointLight(const LightingParams params, const PointLightData pointLightData)
{
	const vec3 unnormalizedLightVector = pointLightData.positionRadius.xyz - params.viewSpacePosition;
	const vec3 L = normalize(unnormalizedLightVector);
	const float att = getDistanceAtt(unnormalizedLightVector, pointLightData.colorInvSqrAttRadius.w);
	const vec3 radiance = pointLightData.colorInvSqrAttRadius.rgb * att;

#if DIFFUSE_ONLY
	return diffuseBrdf(params, radiance, L);
#else
	return cookTorranceSpecularBrdf(params, radiance, L);
#endif // DIFFUSE_ONLY
}

vec3 evaluateDirectionalLight(const LightingParams params, const DirectionalLightData directionalLightData)
{
#if DIFFUSE_ONLY
	return diffuseBrdf(params, directionalLightData.color.rgb, directionalLightData.direction.xyz);
#else
	return cookTorranceSpecularBrdf(params, directionalLightData.color.rgb, directionalLightData.direction.xyz);
#endif // DIFFUSE_ONLY
}

#if SHADOW_FUNCTIONS
float evaluateShadow(sampler2DShadow shadowTexture, vec3 shadowCoord, vec2 pixelCoord, float kernelScale)
{
	float shadow = 0.0;
	
	const float noise = interleavedGradientNoise(pixelCoord);
    
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
    
	const vec2 invShadowTextureSize = 1.0 / textureSize(shadowTexture, 0).xy;
    
	for(int i = 0; i < 8; ++i)
	{
		const vec2 offset = rotation * samples[i];
		shadow += texture(shadowTexture, vec3(shadowCoord.xy + offset * invShadowTextureSize * kernelScale, shadowCoord.z)).x;
	}
	shadow *= 1.0 / 8.0;
	
	return texture(shadowTexture, shadowCoord).x;
}

float evaluateDirectionalLightShadow(const DirectionalLightData directionalLightData, sampler2DArrayShadow shadowTexture, mat4 invViewMatrix, vec3 viewSpacePosition, vec2 pixelCoord, out uint s)
{
	const vec4 offsetPos = invViewMatrix * vec4(0.1 * directionalLightData.direction.xyz + viewSpacePosition, 1.0);
	
	const uint shadowDataOffset = directionalLightData.shadowDataOffsetCount >> 16;
	const uint shadowDataCount = directionalLightData.shadowDataOffsetCount & 0xFFFF;
	// find cascade index
	for (uint i = 0; i < shadowDataCount; ++i)
	{
		uint index = i + shadowDataOffset;
		const vec4 projCoords4 = uShadowMatrices[index] * offsetPos;
		vec3 shadowCoord = (projCoords4.xyz / projCoords4.w);
		shadowCoord.xy = shadowCoord.xy * 0.5 + 0.5;
		if (shadowCoord.x < 0.0 || shadowCoord.x > 1.0 || shadowCoord.y < 0.0 || shadowCoord.y > 1.0)
		{
			continue;
		}
		s = i;
		return texture(uShadowTexture, vec4(shadowCoord.xy, index, shadowCoord.z)).x;// evaluateShadow(shadowTexture, shadowCoord, pixelCoord, 5.0);
	}
	
	s = shadowDataCount;
	return 0.0;
}
#endif // SHADOW_FUNCTIONS

uint getTileAddress(uvec2 pixelCoord, uint width, uint wordCount)
{
	uvec2 tile = pixelCoord / TILE_SIZE;
	uint address = tile.x + tile.y * (width / TILE_SIZE + ((width % TILE_SIZE == 0) ? 0 : 1));
	return address * wordCount;
}

vec3 accurateSRGBToLinear(in vec3 sRGBCol)
{
	vec3 linearRGBLo = sRGBCol * (1.0 / 12.92);
	vec3 linearRGBHi = pow((sRGBCol + vec3(0.055)) * vec3(1.0 / 1.055), vec3(2.4));
	vec3 linearRGB = mix(linearRGBLo, linearRGBHi, vec3(greaterThan(sRGBCol, vec3(0.04045))));
	return linearRGB;
}

vec4 accurateSRGBToLinear(in vec4 sRGBCol)
{
	vec4 linearRGBLo = sRGBCol * (1.0 / 12.92);
	vec4 linearRGBHi = pow((sRGBCol + vec4(0.055)) * vec4(1.0 / 1.055), vec4(2.4));
	vec4 linearRGB = mix(linearRGBLo, linearRGBHi, vec4(greaterThan(sRGBCol, vec4(0.04045))));
	return linearRGB;
}

vec3 accurateLinearToSRGB(in vec3 linearCol)
{
	vec3 sRGBLo = linearCol * 12.92;
	vec3 sRGBHi = (pow(abs(linearCol), vec3(1.0/2.4)) * 1.055) - 0.055;
	vec3 sRGB = mix(sRGBLo, sRGBHi, vec3(greaterThan(linearCol, vec3(0.0031308))));
	return sRGB;
}

vec4 accurateLinearToSRGB(in vec4 linearCol)
{
	vec4 sRGBLo = linearCol * 12.92;
	vec4 sRGBHi = (pow(abs(linearCol), vec4(1.0/2.4)) * 1.055) - 0.055;
	vec4 sRGB = mix(sRGBLo, sRGBHi, vec4(greaterThan(linearCol, vec4(0.0031308))));
	return sRGB;
}
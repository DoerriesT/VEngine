#include "bindingHelper.hlsli"
#include "forward.hlsli"
#include "packing.hlsli"
#include "common.hlsli"
#include "lighting.hlsli"
#include "srgb.hlsli"
#include "commonFilter.hlsli"

#define VOLUME_DEPTH (64)
#define VOLUME_NEAR (0.5)
#define VOLUME_FAR (64.0)

struct inputput
{
	float4 position : SV_Position;
	float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
	float4 worldPos : WORLD_POSITION;
	nointerpolation uint materialIndex : MATERIAL_INDEX;
};

struct PSOutput
{
	float4 color : SV_Target0;
	float4 normal : SV_Target1;
	float4 specularRoughness : SV_Target2;
};

ConstantBuffer<Constants> g_Constants : REGISTER_CBV(CONSTANT_BUFFER_BINDING, CONSTANT_BUFFER_SET);
StructuredBuffer<MaterialData> g_MaterialData : REGISTER_SRV(MATERIAL_DATA_BINDING, MATERIAL_DATA_SET);
Texture2D<float> g_AmbientOcclusionImage : REGISTER_SRV(SSAO_IMAGE_BINDING, SSAO_IMAGE_SET);
Texture2DArray<float> g_DeferredShadowImage : REGISTER_SRV(DEFERRED_SHADOW_IMAGE_BINDING, DEFERRED_SHADOW_IMAGE_SET);
Texture2D<float> g_ShadowAtlasImage : REGISTER_SRV(SHADOW_ATLAS_IMAGE_BINDING, SHADOW_ATLAS_IMAGE_SET);
SamplerComparisonState g_ShadowSampler : REGISTER_SAMPLER(SHADOW_SAMPLER_BINDING, SHADOW_SAMPLER_SET);
ByteAddressBuffer g_ExposureData : REGISTER_SRV(EXPOSURE_DATA_BUFFER_BINDING, EXPOSURE_DATA_BUFFER_SET);

// directional lights
StructuredBuffer<DirectionalLight> g_DirectionalLights : REGISTER_SRV(DIRECTIONAL_LIGHTS_BINDING, DIRECTIONAL_LIGHTS_SET);
StructuredBuffer<DirectionalLight> g_DirectionalLightsShadowed : REGISTER_SRV(DIRECTIONAL_LIGHTS_SHADOWED_BINDING, DIRECTIONAL_LIGHTS_SHADOWED_SET);

// punctual lights
StructuredBuffer<PunctualLight> g_PunctualLights : REGISTER_SRV(PUNCTUAL_LIGHTS_BINDING, PUNCTUAL_LIGHTS_SET);
ByteAddressBuffer g_PunctualLightsBitMask : REGISTER_SRV(PUNCTUAL_LIGHTS_BIT_MASK_BINDING, PUNCTUAL_LIGHTS_BIT_MASK_SET);
ByteAddressBuffer g_PunctualLightsDepthBins : REGISTER_SRV(PUNCTUAL_LIGHTS_Z_BINS_BINDING, PUNCTUAL_LIGHTS_Z_BINS_SET);

// punctual lights shadowed
StructuredBuffer<PunctualLightShadowed> g_PunctualLightsShadowed : REGISTER_SRV(PUNCTUAL_LIGHTS_SHADOWED_BINDING, PUNCTUAL_LIGHTS_SHADOWED_SET);
ByteAddressBuffer g_PunctualLightsShadowedBitMask : REGISTER_SRV(PUNCTUAL_LIGHTS_SHADOWED_BIT_MASK_BINDING, PUNCTUAL_LIGHTS_SHADOWED_BIT_MASK_SET);
ByteAddressBuffer g_PunctualLightsShadowedDepthBins : REGISTER_SRV(PUNCTUAL_LIGHTS_SHADOWED_Z_BINS_BINDING, PUNCTUAL_LIGHTS_SHADOWED_Z_BINS_SET);


Texture2D<float4> g_Textures[TEXTURE_ARRAY_SIZE] : REGISTER_SRV(TEXTURES_BINDING, TEXTURES_SET);
SamplerState g_Samplers[SAMPLER_COUNT] : REGISTER_SAMPLER(SAMPLERS_BINDING, SAMPLERS_SET);


// based on http://www.thetenthplanet.de/archives/1180
float3x3 calculateTBN(float3 N, float3 p, float2 uv)
{
    // get edge vectors of the pixel triangle
    float3 dp1 = ddx(p);
    float3 dp2 = ddy(p);
    float2 duv1 = ddx(uv);
    float2 duv2 = ddy(uv);
 
    // solve the linear system
    float3 dp2perp = cross(dp2, N);
    float3 dp1perp = cross(N, dp1);
    float3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    float3 B = dp2perp * duv1.y + dp1perp * duv2.y;
 
    // construct a scale-invariant frame 
    float invmax = rsqrt( max( dot(T,T), dot(B,B) ) );
    return float3x3(T * invmax, B * invmax, N);
}


[earlydepthstencil]
PSOutput main(inputput input)
{
	const MaterialData materialData = g_MaterialData[input.materialIndex];
	
	LightingParams lightingParams;
	lightingParams.viewSpacePosition = input.worldPos.xyz / input.worldPos.w;
	lightingParams.V = -normalize(lightingParams.viewSpacePosition);
	float4 derivatives = float4(ddx(input.texCoord), ddy(input.texCoord));
	
	// albedo
	{
		float3 albedo = unpackUnorm4x8(materialData.albedoOpacity).rgb;
		uint albedoTextureIndex = (materialData.albedoNormalTexture & 0xFFFF0000) >> 16;
		if (albedoTextureIndex != 0)
		{
			albedo = g_Textures[NonUniformResourceIndex(albedoTextureIndex - 1)].SampleGrad(g_Samplers[SAMPLER_LINEAR_REPEAT], input.texCoord, derivatives.xy, derivatives.zw).rgb;
		}
		lightingParams.albedo = accurateSRGBToLinear(albedo);
	}
	
	float3 normal = normalize(input.normal);
	float3 normalWS =  mul(g_Constants.invViewMatrix, float4(normal, 0.0)).xyz;
	// normal
	{
		
		uint normalTextureIndex = (materialData.albedoNormalTexture & 0xFFFF);
		if (normalTextureIndex != 0)
		{
			float3 tangentSpaceNormal;
			tangentSpaceNormal.xy = g_Textures[NonUniformResourceIndex(normalTextureIndex - 1)].SampleGrad(g_Samplers[SAMPLER_LINEAR_REPEAT], input.texCoord, derivatives.xy, derivatives.zw).xy * 2.0 - 1.0;
			tangentSpaceNormal.z = sqrt(1.0 - tangentSpaceNormal.x * tangentSpaceNormal.x + tangentSpaceNormal.y * tangentSpaceNormal.y);
			normal = mul(tangentSpaceNormal, calculateTBN(normal, lightingParams.viewSpacePosition, float2(input.texCoord.x, -input.texCoord.y)));
			normal = normalize(normal);
		}
		lightingParams.N = normal;
	}
	
	// metalness
	{
		float metalness = unpackUnorm4x8(materialData.metalnessRoughness).x;
		uint metalnessTextureIndex = (materialData.metalnessRoughnessTexture & 0xFFFF0000) >> 16;
		if (metalnessTextureIndex != 0)
		{
			metalness = g_Textures[NonUniformResourceIndex(metalnessTextureIndex - 1)].SampleGrad(g_Samplers[SAMPLER_LINEAR_REPEAT], input.texCoord, derivatives.xy, derivatives.zw).z;
		}
		lightingParams.metalness = metalness;
	}
	
	// roughness
	{
		float roughness = unpackUnorm4x8(materialData.metalnessRoughness).y;
		uint roughnessTextureIndex = (materialData.metalnessRoughnessTexture & 0xFFFF);
		if (roughnessTextureIndex != 0)
		{
			roughness = g_Textures[NonUniformResourceIndex(roughnessTextureIndex - 1)].SampleGrad(g_Samplers[SAMPLER_LINEAR_REPEAT], input.texCoord, derivatives.xy, derivatives.zw).y;
		}
		lightingParams.roughness = roughness;
	}
	
	float3 worldSpacePos = mul(g_Constants.invViewMatrix, float4(lightingParams.viewSpacePosition, 1.0)).xyz;
	
	
	float3 result = 0.0;
	float ao = 1.0;
	if (g_Constants.ambientOcclusion != 0)
	{
		ao = g_AmbientOcclusionImage.Load(int3((int2)input.position.xy, 0)).x;
	}
	result = 1.0 * (1.0 - lightingParams.metalness) * lightingParams.albedo * ao;
	
	// directional lights
	{
		for (uint i = 0; i < g_Constants.directionalLightCount; ++i)
		{
			result += evaluateDirectionalLight(lightingParams, g_DirectionalLights[i]);
		}
	}
	
	// shadowed directional lights
	{
		for (uint i = 0; i < g_Constants.directionalLightShadowedCount; ++i)
		{
			const float3 contribution = evaluateDirectionalLight(lightingParams, g_DirectionalLightsShadowed[i]);
			result += contribution * g_DeferredShadowImage.Load(int4((int2)input.position.xy, i, 0)).x;
		}
	}
	
	// punctual lights
	uint punctualLightCount = g_Constants.punctualLightCount;
	if (punctualLightCount > 0)
	{
		uint wordMin, wordMax, minIndex, maxIndex, wordCount;
		getLightingMinMaxIndices(g_PunctualLightsDepthBins, punctualLightCount, -lightingParams.viewSpacePosition.z, minIndex, maxIndex, wordMin, wordMax, wordCount);
		const uint address = getTileAddress(uint2(input.position.xy), g_Constants.width, wordCount);

		for (uint wordIndex = wordMin; wordIndex <= wordMax; ++wordIndex)
		{
			uint mask = getLightingBitMask(g_PunctualLightsBitMask, address, wordIndex, minIndex, maxIndex);
			
			while (mask != 0)
			{
				const uint bitIndex = firstbitlow(mask);
				const uint index = 32 * wordIndex + bitIndex;
				mask ^= (1 << bitIndex);
				result += evaluatePunctualLight(lightingParams, g_PunctualLights[index]);
			}
		}
	}
	
	// punctual lights shadowed
	uint punctualLightShadowedCount = g_Constants.punctualLightShadowedCount;
	if (punctualLightShadowedCount > 0)
	{
		uint wordMin, wordMax, minIndex, maxIndex, wordCount;
		getLightingMinMaxIndices(g_PunctualLightsShadowedDepthBins, punctualLightShadowedCount, -lightingParams.viewSpacePosition.z, minIndex, maxIndex, wordMin, wordMax, wordCount);
		const uint address = getTileAddress(uint2(input.position.xy), g_Constants.width, wordCount);

		for (uint wordIndex = wordMin; wordIndex <= wordMax; ++wordIndex)
		{
			uint mask = getLightingBitMask(g_PunctualLightsShadowedBitMask, address, wordIndex, minIndex, maxIndex);
			
			while (mask != 0)
			{
				const uint bitIndex = firstbitlow(mask);
				const uint index = 32 * wordIndex + bitIndex;
				mask ^= (1 << bitIndex);
				
				PunctualLightShadowed lightShadowed = g_PunctualLightsShadowed[index];
				
				// evaluate shadow
				float4 shadowPosWS = float4(worldSpacePos + normalWS * 0.05, 1.0);
				float4 shadowPos;
				
				// spot light
				if (lightShadowed.light.angleScale != -1.0)
				{
					shadowPos.x = dot(lightShadowed.shadowMatrix0, shadowPosWS);
					shadowPos.y = dot(lightShadowed.shadowMatrix1, shadowPosWS);
					shadowPos.z = dot(lightShadowed.shadowMatrix2, shadowPosWS);
					shadowPos.w = dot(lightShadowed.shadowMatrix3, shadowPosWS);
					shadowPos.xyz /= shadowPos.w;
					shadowPos.xy = shadowPos.xy * 0.5 + 0.5;
					shadowPos.xy = shadowPos.xy * lightShadowed.shadowAtlasParams[0].x + lightShadowed.shadowAtlasParams[0].yz;
				}
				// point light
				else
				{
					float3 lightToPoint = shadowPosWS.xyz - lightShadowed.positionWS;
					int faceIdx = 0;
					shadowPos.xy = sampleCube(lightToPoint, faceIdx);
					shadowPos.x = 1.0 - shadowPos.x; // correct for handedness (cubemap coordinate system is left-handed, our world space is right-handed)
					shadowPos.xy = shadowPos.xy * lightShadowed.shadowAtlasParams[faceIdx].x + lightShadowed.shadowAtlasParams[faceIdx].yz;
					
					float dist = faceIdx < 2 ? abs(lightToPoint.x) : faceIdx < 4 ? abs(lightToPoint.y) : abs(lightToPoint.z);
					const float nearPlane = 0.1f;
					float param0 = -lightShadowed.radius / (lightShadowed.radius - nearPlane);
					float param1 = param0 * nearPlane;
					shadowPos.z = -param0 + param1 / dist;
				}
				
				float shadow = g_ShadowAtlasImage.SampleCmpLevelZero(g_ShadowSampler, shadowPos.xy, shadowPos.z).x;
				
				result += shadow * evaluatePunctualLight(lightingParams, lightShadowed.light);
			}
		}
	}
	
	// apply pre-exposure
	result *= asfloat(g_ExposureData.Load(0));

	PSOutput output;
	
	output.color = float4(result, 1.0);
	output.normal = float4(lightingParams.N, 1.0);
	output.specularRoughness = approximateLinearToSRGB(float4(lerp(0.04, lightingParams.albedo, lightingParams.metalness), lightingParams.roughness));
	
	return output;
}
#include "bindingHelper.hlsli"
#include "forward.hlsli"
#include "packing.hlsli"
#include "common.hlsli"
#define SPECULAR_AA 1
#include "lighting.hlsli"
#include "srgb.hlsli"
#include "commonFilter.hlsli"
#include "commonEncoding.hlsli"
#include "commonFourierOpacity.hlsli"

#define VOLUME_DEPTH (64)
#define VOLUME_NEAR (0.5)
#define VOLUME_FAR (64.0)

struct PSInput
{
	float4 position : SV_Position;
	float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
	float4 tangent : TANGENT;
	float3 worldPos : WORLD_POSITION;
	nointerpolation uint materialIndex : MATERIAL_INDEX;
};

struct PSOutput
{
	float4 color : SV_Target0;
	float4 normalRoughness : SV_Target1;
	float4 albedoMetalness : SV_Target2;
};

ConstantBuffer<Constants> g_Constants : REGISTER_CBV(CONSTANT_BUFFER_BINDING, 0);
StructuredBuffer<MaterialData> g_MaterialData : REGISTER_SRV(MATERIAL_DATA_BINDING, 0);
Texture2D<float> g_AmbientOcclusionImage : REGISTER_SRV(SSAO_IMAGE_BINDING, 0);
Texture2DArray<float> g_DeferredShadowImage : REGISTER_SRV(DEFERRED_SHADOW_IMAGE_BINDING, 0);
Texture2D<float> g_ShadowAtlasImage : REGISTER_SRV(SHADOW_ATLAS_IMAGE_BINDING, 0);
ByteAddressBuffer g_ExposureData : REGISTER_SRV(EXPOSURE_DATA_BUFFER_BINDING, 0);
Texture2DArray<float4> g_FomImage : REGISTER_SRV(FOM_IMAGE_BINDING, 0);

// directional lights
StructuredBuffer<DirectionalLight> g_DirectionalLights : REGISTER_SRV(DIRECTIONAL_LIGHTS_BINDING, 0);
StructuredBuffer<DirectionalLight> g_DirectionalLightsShadowed : REGISTER_SRV(DIRECTIONAL_LIGHTS_SHADOWED_BINDING, 0);

// punctual lights
StructuredBuffer<PunctualLight> g_PunctualLights : REGISTER_SRV(PUNCTUAL_LIGHTS_BINDING, 0);
Texture2DArray<uint> g_PunctualLightsBitMaskImage : REGISTER_SRV(PUNCTUAL_LIGHTS_BIT_MASK_BINDING, 0);
//ByteAddressBuffer g_PunctualLightsBitMask : REGISTER_SRV(PUNCTUAL_LIGHTS_BIT_MASK_BINDING, 0);
ByteAddressBuffer g_PunctualLightsDepthBins : REGISTER_SRV(PUNCTUAL_LIGHTS_Z_BINS_BINDING, 0);

// punctual lights shadowed
StructuredBuffer<PunctualLightShadowed> g_PunctualLightsShadowed : REGISTER_SRV(PUNCTUAL_LIGHTS_SHADOWED_BINDING, 0);
Texture2DArray<uint> g_PunctualLightsShadowedBitMaskImage : REGISTER_SRV(PUNCTUAL_LIGHTS_SHADOWED_BIT_MASK_BINDING, 0);
//ByteAddressBuffer g_PunctualLightsShadowedBitMask : REGISTER_SRV(PUNCTUAL_LIGHTS_SHADOWED_BIT_MASK_BINDING, 0);
ByteAddressBuffer g_PunctualLightsShadowedDepthBins : REGISTER_SRV(PUNCTUAL_LIGHTS_SHADOWED_Z_BINS_BINDING, 0);


Texture2D<float4> g_Textures[TEXTURE_ARRAY_SIZE] : REGISTER_SRV(0, 1);

SamplerState g_Samplers[SAMPLER_COUNT] : REGISTER_SAMPLER(0, 2);

SamplerComparisonState g_ShadowSampler : REGISTER_SAMPLER(0, 3);

[earlydepthstencil]
PSOutput main(PSInput input)
{
	const MaterialData materialData = g_MaterialData[input.materialIndex];
	
	LightingParams lightingParams;
	lightingParams.position = input.worldPos.xyz;
	lightingParams.V = normalize(g_Constants.cameraPos - lightingParams.position);
	
	// albedo
	{
		float3 albedo = unpackUnorm4x8(materialData.albedoOpacity).rgb;
		uint albedoTextureIndex = (materialData.albedoNormalTexture & 0xFFFF0000) >> 16;
		if (albedoTextureIndex != 0)
		{
			albedo = g_Textures[(albedoTextureIndex - 1)].SampleBias(g_Samplers[SAMPLER_LINEAR_REPEAT], input.texCoord, g_Constants.lodBias).rgb;
		}
		lightingParams.albedo = accurateSRGBToLinear(albedo);
	}
	
	float3 normal = normalize(input.normal);
	float3 vertexNormal = normal;
	// normal
	{
		
		uint normalTextureIndex = (materialData.albedoNormalTexture & 0xFFFF);
		if (normalTextureIndex != 0)
		{
			float3 tangentSpaceNormal;
			tangentSpaceNormal.xy = g_Textures[(normalTextureIndex - 1)].SampleBias(g_Samplers[SAMPLER_LINEAR_REPEAT], input.texCoord, g_Constants.lodBias).xy * 2.0 - 1.0;
			tangentSpaceNormal.z = sqrt(1.0 - tangentSpaceNormal.x * tangentSpaceNormal.x + tangentSpaceNormal.y * tangentSpaceNormal.y);

			float3 bitangent = cross(input.normal, input.tangent.xyz) * input.tangent.w;
			normal = tangentSpaceNormal.x * input.tangent.xyz + tangentSpaceNormal.y * bitangent + tangentSpaceNormal.z * input.normal;
			
			normal = normalize(normal);
			normal = any(isnan(normal)) ? normalize(input.normal) : normal;
		}
		lightingParams.N = normal;
	}
	
	// metalness
	{
		float metalness = unpackUnorm4x8(materialData.metalnessRoughness).x;
		uint metalnessTextureIndex = (materialData.metalnessRoughnessTexture & 0xFFFF0000) >> 16;
		if (metalnessTextureIndex != 0)
		{
			metalness = g_Textures[(metalnessTextureIndex - 1)].SampleBias(g_Samplers[SAMPLER_LINEAR_REPEAT], input.texCoord, g_Constants.lodBias).z;
			metalness = accurateSRGBToLinear(metalness.xxx).x;
		}
		lightingParams.metalness = metalness;
	}
	
	// roughness
	{
		float roughness = unpackUnorm4x8(materialData.metalnessRoughness).y;
		uint roughnessTextureIndex = (materialData.metalnessRoughnessTexture & 0xFFFF);
		if (roughnessTextureIndex != 0)
		{
			roughness = g_Textures[(roughnessTextureIndex - 1)].SampleBias(g_Samplers[SAMPLER_LINEAR_REPEAT], input.texCoord, g_Constants.lodBias).y;
			roughness = accurateSRGBToLinear(roughness.xxx).x;
		}
		lightingParams.roughness = roughness;
	}
	
	float3 result = 0.0;
	//float ao = 1.0;
	//if (g_Constants.ambientOcclusion != 0)
	//{
	//	ao = g_AmbientOcclusionImage.Load(int3((int2)input.position.xy, 0)).x;
	//}
	//result = (1.0 / PI) * (1.0 - lightingParams.metalness) * lightingParams.albedo * ao;
	
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
	
	const float linearDepth = -dot(g_Constants.viewMatrixDepthRow, float4(lightingParams.position, 1.0));
	
	// punctual lights
	uint punctualLightCount = g_Constants.punctualLightCount;
	if (punctualLightCount > 0)
	{
		uint wordMin, wordMax, minIndex, maxIndex, wordCount;
		getLightingMinMaxIndices(g_PunctualLightsDepthBins, punctualLightCount, linearDepth, minIndex, maxIndex, wordMin, wordMax, wordCount);
		//const uint address = getTileAddress(uint2(input.position.xy), g_Constants.width, wordCount);
		const uint2 tile = getTile(int2(input.position.xy));

		for (uint wordIndex = wordMin; wordIndex <= wordMax; ++wordIndex)
		{
			uint mask = getLightingBitMask(g_PunctualLightsBitMaskImage, tile, wordIndex, minIndex, maxIndex);
			
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
		getLightingMinMaxIndices(g_PunctualLightsShadowedDepthBins, punctualLightShadowedCount, linearDepth, minIndex, maxIndex, wordMin, wordMax, wordCount);
		//const uint address = getTileAddress(uint2(input.position.xy), g_Constants.width, wordCount);
		const uint2 tile = getTile(int2(input.position.xy));

		for (uint wordIndex = wordMin; wordIndex <= wordMax; ++wordIndex)
		{
			uint mask = getLightingBitMask(g_PunctualLightsShadowedBitMaskImage, tile, wordIndex, minIndex, maxIndex);
			
			while (mask != 0)
			{
				const uint bitIndex = firstbitlow(mask);
				const uint index = 32 * wordIndex + bitIndex;
				mask ^= (1 << bitIndex);
				
				PunctualLightShadowed lightShadowed = g_PunctualLightsShadowed[index];
				bool isSpotLight = lightShadowed.light.angleScale != -1.0;
				
				// evaluate shadow
				float4 shadowPosWS = float4(lightingParams.position + vertexNormal * 0.05, 1.0);
				float4 shadowPos;
				
				// spot light
				if (isSpotLight)
				{
					shadowPos.x = dot(lightShadowed.shadowMatrix0, shadowPosWS);
					shadowPos.y = dot(lightShadowed.shadowMatrix1, shadowPosWS);
					shadowPos.z = dot(lightShadowed.shadowMatrix2, shadowPosWS);
					shadowPos.w = dot(lightShadowed.shadowMatrix3, shadowPosWS);
					shadowPos.xyz /= shadowPos.w;
					shadowPos.xy = shadowPos.xy * float2(0.5, -0.5) + 0.5;
					shadowPos.xy = shadowPos.xy * lightShadowed.shadowAtlasParams[0].x + lightShadowed.shadowAtlasParams[0].yz;
				}
				// point light
				else
				{
					float3 lightToPoint = shadowPosWS.xyz - lightShadowed.light.position;
					int faceIdx = 0;
					shadowPos.xy = sampleCube(lightToPoint, faceIdx);
					// scale down the coord to account for the border area required for filtering
					shadowPos.xy = ((shadowPos.xy * 2.0 - 1.0) * lightShadowed.shadowAtlasParams[faceIdx].w) * 0.5 + 0.5;
					shadowPos.x = 1.0 - shadowPos.x; // correct for handedness (cubemap coordinate system is left-handed, our world space is right-handed)
					shadowPos.xy = shadowPos.xy * lightShadowed.shadowAtlasParams[faceIdx].x + lightShadowed.shadowAtlasParams[faceIdx].yz;
					
					float dist = faceIdx < 2 ? abs(lightToPoint.x) : faceIdx < 4 ? abs(lightToPoint.y) : abs(lightToPoint.z);
					const float nearPlane = 0.1f;
					float param0 = -lightShadowed.radius / (lightShadowed.radius - nearPlane);
					float param1 = param0 * nearPlane;
					shadowPos.z = -param0 + param1 / dist;
				}
				
				float shadow = g_ShadowAtlasImage.SampleCmpLevelZero(g_ShadowSampler, shadowPos.xy, shadowPos.z).x;
				
				if (shadow > 0.0 && g_Constants.volumetricShadow && lightShadowed.fomShadowAtlasParams.w != 0.0)
				{
					float2 uv;
					// spot light
					if (isSpotLight)
					{
						uv.x = dot(lightShadowed.shadowMatrix0, float4(lightingParams.position, 1.0));
						uv.y = dot(lightShadowed.shadowMatrix1, float4(lightingParams.position, 1.0));
						uv /= dot(lightShadowed.shadowMatrix3, float4(lightingParams.position, 1.0));
						uv = uv * float2(0.5, -0.5) + 0.5;
					}
					// point light
					else
					{
						uv = encodeOctahedron(normalize(lightShadowed.light.position - lightingParams.position)) * 0.5 + 0.5;
					}
					
					uv = uv * lightShadowed.fomShadowAtlasParams.x + lightShadowed.fomShadowAtlasParams.yz;
					
					float4 fom0 = g_FomImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], float3(uv, 0.0), 0.0);
					float4 fom1 = g_FomImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], float3(uv, 1.0), 0.0);
					
					float depth = distance(lightingParams.position, lightShadowed.light.position) * rcp(lightShadowed.radius);
					//depth = saturate(depth);
					
					shadow = min(shadow, fourierOpacityGetTransmittance(depth, fom0, fom1));
				}
				
				result += shadow * evaluatePunctualLight(lightingParams, lightShadowed.light);
			}
		}
	}
	
	// apply pre-exposure
	result *= asfloat(g_ExposureData.Load(0));

	PSOutput output;
	
	output.color = float4(dot(input.tangent, input.tangent) != 0.0 ? result : float3(1.0, 0.0, 0.0), 1.0);
	output.normalRoughness = float4(encodeOctahedron24(lightingParams.N), lightingParams.roughness);
	output.albedoMetalness = approximateLinearToSRGB(float4(lightingParams.albedo, lightingParams.metalness));
	
	return output;
}
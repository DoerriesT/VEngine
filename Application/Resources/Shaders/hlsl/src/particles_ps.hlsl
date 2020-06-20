#include "bindingHelper.hlsli"
#include "particles.hlsli"
#include "common.hlsli"
#include "commonEncoding.hlsli"
#include "commonFourierOpacity.hlsli"
#include "lighting.hlsli"
#include "commonFilter.hlsli"
#include "srgb.hlsli"

#define VOLUME_DEPTH (64)
#define VOLUME_NEAR (0.5)
#define VOLUME_FAR (64.0)

struct PSInput
{
	float4 position : SV_Position;
	float2 texCoord : TEXCOORD;
	float opacity : OPACITY;
	float3 worldSpacePos : WORLD_SPACE_POS;
	float3 viewSpacePos : VIEW_SPACE_POS;
	nointerpolation uint textureIndex : TEXTURE_INDEX;
};

struct PSOutput
{
	float4 color : SV_Target0;
};

ConstantBuffer<Constants> g_Constants : REGISTER_CBV(CONSTANT_BUFFER_BINDING, 0);
Texture2D<float> g_ShadowAtlasImage : REGISTER_SRV(SHADOW_ATLAS_IMAGE_BINDING, 0);
SamplerComparisonState g_ShadowSampler : REGISTER_SAMPLER(SHADOW_SAMPLER_BINDING, 0);
ByteAddressBuffer g_ExposureData : REGISTER_SRV(EXPOSURE_DATA_BUFFER_BINDING, 0);
Texture2DArray<float4> g_fomImage : REGISTER_SRV(FOM_IMAGE_BINDING, 0);
StructuredBuffer<float4x4> g_ShadowMatrices : REGISTER_SRV(SHADOW_MATRICES_BINDING, 0);
Texture2DArray<float4> g_ShadowImage : REGISTER_SRV(SHADOW_IMAGE_BINDING, 0);
Texture3D<float4> g_VolumetricFogImage : REGISTER_SRV(VOLUMETRIC_FOG_IMAGE_BINDING, 0);

// directional lights
StructuredBuffer<DirectionalLight> g_DirectionalLights : REGISTER_SRV(DIRECTIONAL_LIGHTS_BINDING, 0);
StructuredBuffer<DirectionalLight> g_DirectionalLightsShadowed : REGISTER_SRV(DIRECTIONAL_LIGHTS_SHADOWED_BINDING, 0);

// punctual lights
StructuredBuffer<PunctualLight> g_PunctualLights : REGISTER_SRV(PUNCTUAL_LIGHTS_BINDING, 0);
ByteAddressBuffer g_PunctualLightsBitMask : REGISTER_SRV(PUNCTUAL_LIGHTS_BIT_MASK_BINDING, 0);
ByteAddressBuffer g_PunctualLightsDepthBins : REGISTER_SRV(PUNCTUAL_LIGHTS_Z_BINS_BINDING, 0);

// punctual lights shadowed
StructuredBuffer<PunctualLightShadowed> g_PunctualLightsShadowed : REGISTER_SRV(PUNCTUAL_LIGHTS_SHADOWED_BINDING, 0);
ByteAddressBuffer g_PunctualLightsShadowedBitMask : REGISTER_SRV(PUNCTUAL_LIGHTS_SHADOWED_BIT_MASK_BINDING, 0);
ByteAddressBuffer g_PunctualLightsShadowedDepthBins : REGISTER_SRV(PUNCTUAL_LIGHTS_SHADOWED_Z_BINS_BINDING, 0);

Texture2D<float4> g_Textures[TEXTURE_ARRAY_SIZE] : REGISTER_SRV(0, 1);
SamplerState g_Samplers[SAMPLER_COUNT] : REGISTER_SAMPLER(1, 1);

float getDirectionalLightShadow(const DirectionalLight directionalLight, float3 worldSpacePos)
{
	float4 tc = -1.0;
	const int cascadeDataOffset = directionalLight.shadowOffset;
	int cascadeCount = directionalLight.shadowCount;
	for (int i = cascadeCount - 1; i >= 0; --i)
	{
		const float4 shadowCoord = float4(mul(g_ShadowMatrices[cascadeDataOffset + i], float4(worldSpacePos, 1.0)).xyz, cascadeDataOffset + i);
		const bool valid = all(abs(shadowCoord.xy) < 0.97);
		tc = valid ? shadowCoord : tc;
	}
	
	tc.xy = tc.xy * float2(0.5, -0.5) + 0.5;
	const float shadow = tc.w != -1.0 ? g_ShadowImage.SampleCmpLevelZero(g_ShadowSampler, tc.xyw, tc.z) : 0.0;
	
	return shadow;
}

[earlydepthstencil]
PSOutput main(PSInput input)
{
	float4 albedoOpacity = 1.0;
	if (input.textureIndex != 0)
	{
		albedoOpacity = g_Textures[input.textureIndex - 1].Sample(g_Samplers[SAMPLER_LINEAR_REPEAT], input.texCoord);
		albedoOpacity.rgb = accurateSRGBToLinear(albedoOpacity.rgb);
	}
	albedoOpacity.a *= input.opacity;
	
	float3 albedo = albedoOpacity.rgb;
	float opacity = albedoOpacity.a;
	
	float3 worldSpacePos = input.worldSpacePos;
	float3 viewSpacePos = input.viewSpacePos;
	
	float3 result = 0.0;
	
	// ambient
	{
		float3 ambientLight = (1.0 / PI) * albedo;
		result += ambientLight;
	}
	
	// directional lights
	{
		for (uint i = 0; i < g_Constants.directionalLightCount; ++i)
		{
			DirectionalLight directionalLight = g_DirectionalLights[i];
			result += directionalLight.color * albedo;
		}
	}
	
	// shadowed directional lights
	{
		for (uint i = 0; i < g_Constants.directionalLightShadowedCount; ++i)
		{
			DirectionalLight directionalLight = g_DirectionalLightsShadowed[i];
			float shadow = getDirectionalLightShadow(directionalLight, worldSpacePos);
			result += directionalLight.color * albedo * shadow;
		}
	}
	
	// punctual lights
	uint punctualLightCount = g_Constants.punctualLightCount;
	if (punctualLightCount > 0)
	{
		uint wordMin, wordMax, minIndex, maxIndex, wordCount;
		getLightingMinMaxIndices(g_PunctualLightsDepthBins, punctualLightCount, -viewSpacePos.z, minIndex, maxIndex, wordMin, wordMax, wordCount);
		const uint address = getTileAddress(input.position.xy, g_Constants.width, wordCount);
	
		for (uint wordIndex = wordMin; wordIndex <= wordMax; ++wordIndex)
		{
			uint mask = getLightingBitMask(g_PunctualLightsBitMask, address, wordIndex, minIndex, maxIndex);
			
			while (mask != 0)
			{
				const uint bitIndex = firstbitlow(mask);
				const uint index = 32 * wordIndex + bitIndex;
				mask ^= (1 << bitIndex);
				
				PunctualLight light = g_PunctualLights[index];
				
				const float3 unnormalizedLightVector = light.position - viewSpacePos;
				const float3 L = normalize(unnormalizedLightVector);
				float att = getDistanceAtt(unnormalizedLightVector, light.invSqrAttRadius);
				
				if (light.angleScale != -1.0) // -1.0 is a special value that marks this light as a point light
				{
					att *= getAngleAtt(L, light.direction, light.angleScale, light.angleOffset);
				}
				result += light.color * albedo * att;
			}
		}
	}
	
	// punctual lights shadowed
	uint punctualLightShadowedCount = g_Constants.punctualLightShadowedCount;
	if (punctualLightShadowedCount > 0)
	{
		uint wordMin, wordMax, minIndex, maxIndex, wordCount;
		getLightingMinMaxIndices(g_PunctualLightsShadowedDepthBins, punctualLightShadowedCount, -viewSpacePos.z, minIndex, maxIndex, wordMin, wordMax, wordCount);
		const uint address = getTileAddress(input.position.xy, g_Constants.width, wordCount);
	
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
				float4 shadowPos;
				
				// spot light
				if (lightShadowed.light.angleScale != -1.0)
				{
					shadowPos.x = dot(lightShadowed.shadowMatrix0, float4(worldSpacePos, 1.0));
					shadowPos.y = dot(lightShadowed.shadowMatrix1, float4(worldSpacePos, 1.0));
					shadowPos.z = dot(lightShadowed.shadowMatrix2, float4(worldSpacePos, 1.0));
					shadowPos.w = dot(lightShadowed.shadowMatrix3, float4(worldSpacePos, 1.0));
					shadowPos.xyz /= shadowPos.w;
					shadowPos.xy = shadowPos.xy * float2(0.5, -0.5) + 0.5;
					shadowPos.xy = shadowPos.xy * lightShadowed.shadowAtlasParams[0].x + lightShadowed.shadowAtlasParams[0].yz;
				}
				// point light
				else
				{
					float3 lightToPoint = worldSpacePos - lightShadowed.positionWS;
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
				
				const float3 unnormalizedLightVector = lightShadowed.light.position - viewSpacePos;
				const float3 L = normalize(unnormalizedLightVector);
				float att = getDistanceAtt(unnormalizedLightVector, lightShadowed.light.invSqrAttRadius);
				
				if (lightShadowed.light.angleScale != -1.0) // -1.0 is a special value that marks this light as a point light
				{
					att *= getAngleAtt(L, lightShadowed.light.direction, lightShadowed.light.angleScale, lightShadowed.light.angleOffset);
				}
				
				// trace extinction volume
				if (shadow > 0.0 && att > 0.0 && g_Constants.volumetricShadow && lightShadowed.fomShadowAtlasParams.w != 0.0)
				{
					float2 uv;
					// spot light
					if (lightShadowed.light.angleScale != -1.0)
					{
						uv.x = dot(lightShadowed.shadowMatrix0, float4(worldSpacePos, 1.0));
						uv.y = dot(lightShadowed.shadowMatrix1, float4(worldSpacePos, 1.0));
						uv /= dot(lightShadowed.shadowMatrix3, float4(worldSpacePos, 1.0));
					}
					// point light
					else
					{
						uv = encodeOctahedron(normalize(lightShadowed.positionWS - worldSpacePos));
					}
					
					uv = uv * 0.5 + 0.5;
					uv = uv * lightShadowed.fomShadowAtlasParams.x + lightShadowed.fomShadowAtlasParams.yz;
					
					float4 fom0 = g_fomImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_REPEAT], float3(uv, 0.0), 0.0);
					float4 fom1 = g_fomImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_REPEAT], float3(uv, 1.0), 0.0);
					
					float depth = distance(worldSpacePos, lightShadowed.positionWS) * rcp(lightShadowed.radius);
					//depth = saturate(depth);
					
					shadow *= fourierOpacityGetTransmittance(depth, fom0, fom1);
				}
				
				result += shadow * lightShadowed.light.color * albedo * att;
			}
		}
	}
	
	result *= (1.0 / (4.0 * PI));
	
	// apply pre-exposure
	result *= asfloat(g_ExposureData.Load(0));
	
	// volumetric fog
	{
		float z = -viewSpacePos.z;
		float d = (log2(max(0, z * (1.0 / VOLUME_NEAR))) * (1.0 / log2(VOLUME_FAR / VOLUME_NEAR)));
		
		// the fog image can extend further to the right/downwards than the lighting image, so we cant just use the uv
		// of the current texel but instead need to scale the uv with respect to the fog image resolution
		float3 imageDims;
		g_VolumetricFogImage.GetDimensions(imageDims.x, imageDims.y, imageDims.z);
		float2 scaledFogImageTexelSize = 1.0 / (imageDims.xy * 8.0);
		
		float3 volumetricFogTexCoord = float3(input.position.xy * scaledFogImageTexelSize, d);
		
		float4 fog = g_VolumetricFogImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], volumetricFogTexCoord, 0.0);
		
		fog.rgb = inverseSimpleTonemap(fog.rgb);
		result = result * fog.aaa + fog.rgb;
	}
	
	PSOutput output = (PSOutput)0;
	output.color = float4(result, opacity);
	
	return output;
}
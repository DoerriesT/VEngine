#include "bindingHelper.hlsli"
#include "forward.hlsli"
#include "packing.hlsli"
#include "common.hlsli"
#include "lighting.hlsli"
#include "srgb.hlsli"

#define VOLUME_DEPTH (64)
#define VOLUME_NEAR (0.5)
#define VOLUME_FAR (64.0)

struct PSInput
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
StructuredBuffer<DirectionalLight> g_DirectionalLights : REGISTER_SRV(DIRECTIONAL_LIGHT_DATA_BINDING, DIRECTIONAL_LIGHT_DATA_SET);
Texture2D<float> g_AmbientOcclusionImage : REGISTER_SRV(SSAO_IMAGE_BINDING, SSAO_IMAGE_SET);
Texture2DArray<float> g_DeferredShadowImage : REGISTER_SRV(DEFERRED_SHADOW_IMAGE_BINDING, DEFERRED_SHADOW_IMAGE_SET);
Texture3D<float4> g_VolumetricFogImage : REGISTER_SRV(VOLUMETRIC_FOG_IMAGE_BINDING, VOLUMETRIC_FOG_IMAGE_SET);
ByteAddressBuffer g_PointLightZBins : REGISTER_SRV(POINT_LIGHT_Z_BINS_BUFFER_BINDING, POINT_LIGHT_Z_BINS_BUFFER_SET);
ByteAddressBuffer g_PointBitMask : REGISTER_SRV(POINT_LIGHT_BIT_MASK_BUFFER_BINDING, POINT_LIGHT_BIT_MASK_BUFFER_SET);
StructuredBuffer<PointLight> g_PointLights : REGISTER_SRV(POINT_LIGHT_DATA_BINDING, POINT_LIGHT_DATA_SET);
ByteAddressBuffer g_SpotLightZBins : REGISTER_SRV(SPOT_LIGHT_Z_BINS_BUFFER_BINDING, SPOT_LIGHT_Z_BINS_BUFFER_SET);
ByteAddressBuffer g_SpotBitMask : REGISTER_SRV(SPOT_LIGHT_BIT_MASK_BUFFER_BINDING, SPOT_LIGHT_BIT_MASK_BUFFER_SET);
StructuredBuffer<SpotLight> g_SpotLights : REGISTER_SRV(SPOT_LIGHT_DATA_BINDING, SPOT_LIGHT_DATA_SET);
ByteAddressBuffer g_ExposureData : REGISTER_SRV(EXPOSURE_DATA_BUFFER_BINDING, EXPOSURE_DATA_BUFFER_SET);


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
PSOutput main(PSInput psIn)
{
	const MaterialData materialData = g_MaterialData[psIn.materialIndex];
	
	LightingParams lightingParams;
	lightingParams.viewSpacePosition = psIn.worldPos.xyz / psIn.worldPos.w;
	lightingParams.V = -normalize(lightingParams.viewSpacePosition);
	float4 derivatives = float4(ddx(psIn.texCoord), ddy(psIn.texCoord));
	
	// albedo
	{
		float3 albedo = unpackUnorm4x8(materialData.albedoOpacity).rgb;
		uint albedoTextureIndex = (materialData.albedoNormalTexture & 0xFFFF0000) >> 16;
		if (albedoTextureIndex != 0)
		{
			albedo = g_Textures[NonUniformResourceIndex(albedoTextureIndex - 1)].SampleGrad(g_Samplers[SAMPLER_LINEAR_REPEAT], psIn.texCoord, derivatives.xy, derivatives.zw).rgb;
		}
		lightingParams.albedo = accurateSRGBToLinear(albedo);
	}
	
	// normal
	{
		float3 normal = normalize(psIn.normal);
		uint normalTextureIndex = (materialData.albedoNormalTexture & 0xFFFF);
		if (normalTextureIndex != 0)
		{
			float3 tangentSpaceNormal;
			tangentSpaceNormal.xy = g_Textures[NonUniformResourceIndex(normalTextureIndex - 1)].SampleGrad(g_Samplers[SAMPLER_LINEAR_REPEAT], psIn.texCoord, derivatives.xy, derivatives.zw).xy * 2.0 - 1.0;
			tangentSpaceNormal.z = sqrt(1.0 - tangentSpaceNormal.x * tangentSpaceNormal.x + tangentSpaceNormal.y * tangentSpaceNormal.y);
			normal = mul(tangentSpaceNormal, calculateTBN(normal, lightingParams.viewSpacePosition, float2(psIn.texCoord.x, -psIn.texCoord.y)));
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
			metalness = g_Textures[NonUniformResourceIndex(metalnessTextureIndex - 1)].SampleGrad(g_Samplers[SAMPLER_LINEAR_REPEAT], psIn.texCoord, derivatives.xy, derivatives.zw).z;
		}
		lightingParams.metalness = metalness;
	}
	
	// roughness
	{
		float roughness = unpackUnorm4x8(materialData.metalnessRoughness).y;
		uint roughnessTextureIndex = (materialData.metalnessRoughnessTexture & 0xFFFF);
		if (roughnessTextureIndex != 0)
		{
			roughness = g_Textures[NonUniformResourceIndex(roughnessTextureIndex - 1)].SampleGrad(g_Samplers[SAMPLER_LINEAR_REPEAT], psIn.texCoord, derivatives.xy, derivatives.zw).y;
		}
		lightingParams.roughness = roughness;
	}
	
	
	float3 result = 0.0;
	float ao = 1.0;
	if (g_Constants.ambientOcclusion != 0)
	{
		ao = g_AmbientOcclusionImage.Load(int3((int2)psIn.position.xy, 0)).x;
	}
	result = lightingParams.albedo * ao;
	
	// directional lights
	for (uint i = 0; i < g_Constants.directionalLightCount; ++i)
	{
		const DirectionalLight directionalLight = g_DirectionalLights[i];
		const float3 contribution = evaluateDirectionalLight(lightingParams, directionalLight);
		// TODO: dont assume that every directional light has a shadow mask
		result += contribution * (1.0 - g_DeferredShadowImage.Load(int4((int2)psIn.position.xy, i, 0)).x);
	}
	
	// point lights
	uint pointLightCount = g_Constants.pointLightCount & 0xFFFF;
	if (pointLightCount > 0)
	{
		uint wordMin, wordMax, minIndex, maxIndex, wordCount;
		getLightingMinMaxIndices(g_PointLightZBins, pointLightCount, -lightingParams.viewSpacePosition.z, minIndex, maxIndex, wordMin, wordMax, wordCount);
		const uint address = getTileAddress(uint2(psIn.position.xy), g_Constants.width, wordCount);

		for (uint wordIndex = wordMin; wordIndex <= wordMax; ++wordIndex)
		{
			uint mask = getLightingBitMask(g_PointBitMask, address, wordIndex, minIndex, maxIndex);
			
			while (mask != 0)
			{
				const uint bitIndex = firstbitlow(mask);
				const uint index = 32 * wordIndex + bitIndex;
				mask ^= (1 << bitIndex);
				result += evaluatePointLight(lightingParams, g_PointLights[index]);
			}
		}
	}
	
	// spot lights
	uint spotLightCount = (g_Constants.pointLightCount & 0xFFFF0000) >> 16;
	if (spotLightCount > 0)
	{
		uint wordMin, wordMax, minIndex, maxIndex, wordCount;
		getLightingMinMaxIndices(g_SpotLightZBins, spotLightCount, -lightingParams.viewSpacePosition.z, minIndex, maxIndex, wordMin, wordMax, wordCount);
		const uint address = getTileAddress(uint2(psIn.position.xy), g_Constants.width, wordCount);
		
		for (uint wordIndex = wordMin; wordIndex <= wordMax; ++wordIndex)
		{
			uint mask = getLightingBitMask(g_SpotBitMask, address, wordIndex, minIndex, maxIndex);
			
			while (mask != 0)
			{
				const uint bitIndex = firstbitlow(mask);
				const uint index = 32 * wordIndex + bitIndex;
				mask ^= (1 << bitIndex);
				result += evaluateSpotLight(lightingParams, g_SpotLights[index]);
			}
		}
	}
	
	// apply volumetric fog
	{
		float z = -lightingParams.viewSpacePosition.z;
		float d = (log2(max(0, z * (1.0 / VOLUME_NEAR))) * (1.0 / log2(VOLUME_FAR / VOLUME_NEAR)));
		
		// the fog image can extend further to the right/downwards than the lighting image, so we cant just use the uv
		// of the current texel but instead need to scale the uv with respect to the fog image resolution
		uint3 imageDims;
		g_VolumetricFogImage.GetDimensions(imageDims.x, imageDims.y, imageDims.z);
		float2 scaledFogImageTexelSize = 1.0 / (float2(imageDims.xy) * 8.0);
		
		float3 volumetricFogTexCoord = float3(psIn.position.xy * scaledFogImageTexelSize, d);
		float4 fog = g_VolumetricFogImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], volumetricFogTexCoord, 0.0);
		result = result * fog.aaa + fog.rgb;
	}
	
	// apply pre-exposure
	result *= asfloat(g_ExposureData.Load(0));

	PSOutput psOut;
	
	psOut.color = float4(result, 1.0);
	psOut.normal = float4(lightingParams.N, 1.0);
	psOut.specularRoughness = accurateLinearToSRGB(float4(lerp(0.04, lightingParams.albedo, lightingParams.metalness), lightingParams.roughness));
	
	return psOut;
}
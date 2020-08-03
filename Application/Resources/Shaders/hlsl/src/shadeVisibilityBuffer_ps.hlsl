#include "bindingHelper.hlsli"
#include "shadeVisibilityBuffer.hlsli"
#include "packing.hlsli"
#include "common.hlsli"
#define SPECULAR_AA 1
#define SKIP_DERIVATIVES_FUNCTIONS 1
#include "lighting.hlsli"
#include "srgb.hlsli"
#include "commonFilter.hlsli"
#include "commonEncoding.hlsli"
#include "commonFourierOpacity.hlsli"
#include "commonVertex.hlsli"

#define VOLUME_DEPTH (64)
#define VOLUME_NEAR (0.5)
#define VOLUME_FAR (64.0)

struct PSInput
{
	float4 position : SV_Position;
	float2 texCoord : TEXCOORD;
};

struct PSOutput
{
	float4 color : SV_Target0;
	float4 normalRoughness : SV_Target1;
	float4 albedoMetalness : SV_Target2;
};

struct SurfaceData
{
	float3 position;
	float3 normal;
	float4 tangent;
	float2 texCoord;
	uint materialIndex;
	float2 texCoordDdx;
	float2 texCoordDdy;
};

struct SubMeshInfo
{
	uint indexCount;
	uint firstIndex;
	int vertexOffset;
};

RWTexture2D<float4> g_ResultImage : REGISTER_UAV(RESULT_IMAGE_BINDING, 0);
RWTexture2D<float4> g_NormalRoughnessImage : REGISTER_UAV(NORMAL_ROUGHNESS_IMAGE_BINDING, 0);
RWTexture2D<float4> g_AlbedoMetalnessImage : REGISTER_UAV(ALBEDO_METALNESS_IMAGE_BINDING, 0);

ConstantBuffer<Constants> g_Constants : REGISTER_CBV(CONSTANT_BUFFER_BINDING, 0);
StructuredBuffer<MaterialData> g_MaterialData : REGISTER_SRV(MATERIAL_DATA_BINDING, 0);

Texture2D<float4> g_TriangleImage : REGISTER_SRV(TRIANGLE_IMAGE_BINDING, 0);
StructuredBuffer<float4> g_TransformData : REGISTER_SRV(TRANSFORM_DATA_BINDING, 0);
StructuredBuffer<uint> g_Positions : REGISTER_SRV(VERTEX_POSITIONS_BINDING, 0);
StructuredBuffer<uint> g_QTangents : REGISTER_SRV(VERTEX_QTANGENTS_BINDING, 0);
StructuredBuffer<uint> g_TexCoords : REGISTER_SRV(VERTEX_TEXCOORDS_BINDING, 0);
StructuredBuffer<uint> g_IndexBuffer : REGISTER_SRV(INDEX_BUFFER_BINDING, 0);
StructuredBuffer<SubMeshInstanceData> g_InstanceData : REGISTER_SRV(INSTANCE_DATA_BINDING, 0);
StructuredBuffer<SubMeshInfo> g_SubMeshInfo : REGISTER_SRV(SUB_MESH_DATA_BINDING, 0);
StructuredBuffer<float4> g_TexCoordScaleBias : REGISTER_SRV(TEXCOORD_SCALE_BIAS_BINDING, 0);


Texture2D<float> g_AmbientOcclusionImage : REGISTER_SRV(SSAO_IMAGE_BINDING, 0);
Texture2DArray<float> g_DeferredShadowImage : REGISTER_SRV(DEFERRED_SHADOW_IMAGE_BINDING, 0);
Texture2D<float> g_ShadowAtlasImage : REGISTER_SRV(SHADOW_ATLAS_IMAGE_BINDING, 0);
SamplerComparisonState g_ShadowSampler : REGISTER_SAMPLER(SHADOW_SAMPLER_BINDING, 0);
ByteAddressBuffer g_ExposureData : REGISTER_SRV(EXPOSURE_DATA_BUFFER_BINDING, 0);
Texture2DArray<float4> g_FomImage : REGISTER_SRV(FOM_IMAGE_BINDING, 0);

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


SurfaceData loadVertex(uint index, uint transformIndex)
{
	SurfaceData result = (SurfaceData)0;
	result.position = getPosition(index, g_Positions);
	result.texCoord = getTexCoord(index, g_TexCoords);
	
	float4 qtangent = getQTangent(index, g_QTangents);
	result.tangent.w = qtangent.w < 0.0 ? -1.0 : 1.0;
	
	// rotate to world space
	qtangent = quaternionMult(g_TransformData[transformIndex * 4 + 3], qtangent);
	
	// extract normal and tangent
	quaternionToNormalTangent(qtangent, result.normal, result.tangent.xyz);
	
	return result;
}

float __dot3(float3 a, float3 b)
{ 
	return mad(a.x, b.x, mad(a.y, b.y, a.z*b.z));
}

float3 __cross3(float3 a, float3 b)
{ 
	return mad(a, b.yzx, -a.yzx * b).yzx;
}

float3 intersectTriangle(float3 p0, float3 p1, float3 p2, float3 o, float3 d)
{
   float3 eo =  o - p0;
   float3 e2 = p2 - p0;
   float3 e1 = p1 - p0;
   float3 r  = __cross3(d, e2);
   float3 s  = __cross3(eo, e1);
   float iV  = 1.0f / __dot3(r, e1);
   float V1  = __dot3(r, eo);
   float V2  = __dot3(s,  d);
   float b   = V1 * iV;
   float c   = V2 * iV;
   float a   = 1.0f - b - c;
   return float3(a, b, c);
}

SurfaceData computeSurfaceData(uint triangleData, int2 pixelCoord)
{
	uint instanceID = triangleData >> 16;
	
	SubMeshInstanceData instanceData = g_InstanceData[instanceID];
	
	uint triangleID = triangleData & 0xFFFF;
	
	SubMeshInfo submeshInfo = g_SubMeshInfo[instanceData.subMeshIndex];
	
	uint indexBaseOffset = submeshInfo.firstIndex + triangleID * 3;
	bool lower = (indexBaseOffset & 1) == 0;
	indexBaseOffset /= 2;
	uint indices16_0 = g_IndexBuffer[indexBaseOffset + 0];
	uint indices16_1 = g_IndexBuffer[indexBaseOffset + 1];
	
	// get indices from packed 16bit indices
	uint index0 = submeshInfo.vertexOffset + (lower ? (indices16_0 & 0xFFFF) : (indices16_0 >> 16));
	uint index1 = submeshInfo.vertexOffset + (lower ? (indices16_0 >> 16) : (indices16_1 & 0xFFFF));
	uint index2 = submeshInfo.vertexOffset + (lower ? (indices16_1 & 0xFFFF) : (indices16_1 >> 16));
	
	SurfaceData v0 = loadVertex(index0, instanceData.transformIndex);
	SurfaceData v1 = loadVertex(index1, instanceData.transformIndex);
	SurfaceData v2 = loadVertex(index2, instanceData.transformIndex);
	
	float4 texCoordScaleBias = g_TexCoordScaleBias[instanceData.subMeshIndex];
	
	v0.texCoord = v0.texCoord * texCoordScaleBias.xy + texCoordScaleBias.zw;
	v1.texCoord = v1.texCoord * texCoordScaleBias.xy + texCoordScaleBias.zw;
	v2.texCoord = v2.texCoord * texCoordScaleBias.xy + texCoordScaleBias.zw;
	
	float4 transform0 = g_TransformData[instanceData.transformIndex * 4 + 0];
	float4 transform1 = g_TransformData[instanceData.transformIndex * 4 + 1];
	float4 transform2 = g_TransformData[instanceData.transformIndex * 4 + 2];

	float3 p0;
	p0.x = dot(transform0, float4(v0.position, 1.0));
	p0.y = dot(transform1, float4(v0.position, 1.0));
	p0.z = dot(transform2, float4(v0.position, 1.0));
	float3 p1;
	p1.x = dot(transform0, float4(v1.position, 1.0));
	p1.y = dot(transform1, float4(v1.position, 1.0));
	p1.z = dot(transform2, float4(v1.position, 1.0));
	float3 p2;
	p2.x = dot(transform0, float4(v2.position, 1.0));
	p2.y = dot(transform1, float4(v2.position, 1.0));
	p2.z = dot(transform2, float4(v2.position, 1.0));
	
	float3 cameraPosWS = float3(g_Constants.cameraPosWSX, g_Constants.cameraPosWSY, g_Constants.cameraPosWSZ);
	
	float3 d = g_Constants.frustumDirDeltaX * (pixelCoord.x + 0.5)
			+ g_Constants.frustumDirDeltaY * (pixelCoord.y + 0.5)
			+ g_Constants.frustumDirTL;
	
	// barycentric coordinates
	float3 H = intersectTriangle(p0.xyz, p1.xyz, p2.xyz, cameraPosWS, d);
	
	SurfaceData result = (SurfaceData)0;
	result.position = mad(p0.xyz, H.x, mad(p1.xyz, H.y, (p2.xyz * H.z)));
	result.normal = mad(v0.normal, H.x, mad(v1.normal, H.y, (v2.normal * H.z)));
	result.normal = mul((float3x3)g_Constants.viewMatrix, result.normal);
	result.tangent = mad(v0.tangent, H.x, mad(v1.tangent, H.y, (v2.tangent * H.z)));
	result.tangent.xyz = mul((float3x3)g_Constants.viewMatrix, result.tangent.xyz);
	result.texCoord = mad(v0.texCoord,  H.x, mad(v1.texCoord,  H.y, (v2.texCoord  * H.z)));

	// derivatives
	float3 dx = g_Constants.frustumDirDeltaX * (pixelCoord.x + 1.5)
			+ g_Constants.frustumDirDeltaY * (pixelCoord.y + 0.5)
			+ g_Constants.frustumDirTL;
	
	float3 dy = g_Constants.frustumDirDeltaX * (pixelCoord.x + 0.5)
			+ g_Constants.frustumDirDeltaY * (pixelCoord.y + 1.5)
			+ g_Constants.frustumDirTL;
	
	float3 Hx = intersectTriangle(p0.xyz, p1.xyz, p2.xyz, cameraPosWS, dx);
	float3 Hy = intersectTriangle(p0.xyz, p1.xyz, p2.xyz, cameraPosWS, dy);
	
	float2 tCoordDX = mad(v0.texCoord.xy, Hx.x, mad(v1.texCoord.xy, Hx.y, (v2.texCoord.xy * Hx.z)));
	float2 tCoordDY = mad(v0.texCoord.xy, Hy.x, mad(v1.texCoord.xy, Hy.y, (v2.texCoord.xy * Hy.z)));
	result.texCoordDdx = result.texCoord - tCoordDX;
	result.texCoordDdy = result.texCoord - tCoordDY;
	
	result.materialIndex = instanceData.materialIndex;
	
	return result;
}


[earlydepthstencil]
PSOutput main(PSInput input)
{
	uint triangleData = packUnorm4x8(g_TriangleImage.Load(uint3(input.position.xy, 0)));
	
	if (triangleData == ~0u)
	{
		discard;
	}
	
	SurfaceData surfaceData = computeSurfaceData(triangleData, input.position.xy);
	
	const MaterialData materialData = g_MaterialData[surfaceData.materialIndex];
	
	LightingParams lightingParams;
	lightingParams.viewSpacePosition = mul(g_Constants.viewMatrix, float4(surfaceData.position, 1.0)).xyz;
	lightingParams.V = -normalize(lightingParams.viewSpacePosition);
	float4 derivatives = float4(surfaceData.texCoordDdx, surfaceData.texCoordDdy);
	
	// albedo
	{
		float3 albedo = unpackUnorm4x8(materialData.albedoOpacity).rgb;
		uint albedoTextureIndex = (materialData.albedoNormalTexture & 0xFFFF0000) >> 16;
		if (albedoTextureIndex != 0)
		{
			albedo = g_Textures[NonUniformResourceIndex(albedoTextureIndex - 1)].SampleGrad(g_Samplers[SAMPLER_LINEAR_REPEAT], surfaceData.texCoord, derivatives.xy, derivatives.zw).rgb;
		}
		lightingParams.albedo = accurateSRGBToLinear(albedo);
	}
	
	float3 normal = normalize(surfaceData.normal);
	float3 normalWS =  mul(g_Constants.invViewMatrix, float4(normal, 0.0)).xyz;
	// normal
	{
		
		uint normalTextureIndex = (materialData.albedoNormalTexture & 0xFFFF);
		if (normalTextureIndex != 0)
		{
			float3 tangentSpaceNormal;
			tangentSpaceNormal.xy = g_Textures[NonUniformResourceIndex(normalTextureIndex - 1)].SampleGrad(g_Samplers[SAMPLER_LINEAR_REPEAT], surfaceData.texCoord, derivatives.xy, derivatives.zw).xy * 2.0 - 1.0;
			tangentSpaceNormal.z = sqrt(1.0 - tangentSpaceNormal.x * tangentSpaceNormal.x + tangentSpaceNormal.y * tangentSpaceNormal.y);
			
			float3 bitangent = cross(surfaceData.normal, surfaceData.tangent.xyz) * surfaceData.tangent.w;
			normal = tangentSpaceNormal.x * surfaceData.tangent.xyz + tangentSpaceNormal.y * bitangent + tangentSpaceNormal.z * surfaceData.normal;
			
			normal = normalize(normal);
			normal = any(isnan(normal)) ? normalize(surfaceData.normal) : normal;
		}
		lightingParams.N = normal;
	}
	
	// metalness
	{
		float metalness = unpackUnorm4x8(materialData.metalnessRoughness).x;
		uint metalnessTextureIndex = (materialData.metalnessRoughnessTexture & 0xFFFF0000) >> 16;
		if (metalnessTextureIndex != 0)
		{
			metalness = g_Textures[NonUniformResourceIndex(metalnessTextureIndex - 1)].SampleGrad(g_Samplers[SAMPLER_LINEAR_REPEAT], surfaceData.texCoord, derivatives.xy, derivatives.zw).z;
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
			roughness = g_Textures[NonUniformResourceIndex(roughnessTextureIndex - 1)].SampleGrad(g_Samplers[SAMPLER_LINEAR_REPEAT], surfaceData.texCoord, derivatives.xy, derivatives.zw).y;
			roughness = accurateSRGBToLinear(roughness.xxx).x;
		}
		lightingParams.roughness = roughness;
	}
	
	float3 worldSpacePos = mul(g_Constants.invViewMatrix, float4(lightingParams.viewSpacePosition, 1.0)).xyz;
	
	
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
			result += contribution * g_DeferredShadowImage.Load(int4(input.position.xy, i, 0)).x;
		}
	}
	
	// punctual lights
	uint punctualLightCount = g_Constants.punctualLightCount;
	if (punctualLightCount > 0)
	{
		uint wordMin, wordMax, minIndex, maxIndex, wordCount;
		getLightingMinMaxIndices(g_PunctualLightsDepthBins, punctualLightCount, -lightingParams.viewSpacePosition.z, minIndex, maxIndex, wordMin, wordMax, wordCount);
		const uint address = getTileAddress(input.position.xy, g_Constants.width, wordCount);

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
				bool isSpotLight = lightShadowed.light.angleScale != -1.0;
				
				// evaluate shadow
				float4 shadowPosWS = float4(worldSpacePos + normalWS * 0.05, 1.0);
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
					float3 lightToPoint = shadowPosWS.xyz - lightShadowed.positionWS;
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
					//if (g_Constants.volumetricShadow == 2)
					//{
					//	shadow *= raymarch(worldSpacePos, lightShadowed.positionWS);
					//}
					//else
					{
						float2 uv;
						// spot light
						if (isSpotLight)
						{
							uv.x = dot(lightShadowed.shadowMatrix0, float4(worldSpacePos, 1.0));
							uv.y = dot(lightShadowed.shadowMatrix1, float4(worldSpacePos, 1.0));
							uv /= dot(lightShadowed.shadowMatrix3, float4(worldSpacePos, 1.0));
							uv = uv * float2(0.5, -0.5) + 0.5;
						}
						// point light
						else
						{
							uv = encodeOctahedron(normalize(lightShadowed.positionWS - worldSpacePos)) * 0.5 + 0.5;
						}
						
						uv = uv * lightShadowed.fomShadowAtlasParams.x + lightShadowed.fomShadowAtlasParams.yz;
						
						float4 fom0 = g_FomImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], float3(uv, 0.0), 0.0);
						float4 fom1 = g_FomImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], float3(uv, 1.0), 0.0);
						
						float depth = distance(worldSpacePos, lightShadowed.positionWS) * rcp(lightShadowed.radius);
						//depth = saturate(depth);
						
						shadow *= fourierOpacityGetTransmittance(depth, fom0, fom1);
					}
				}
				
				result += shadow * evaluatePunctualLight(lightingParams, lightShadowed.light);
			}
		}
	}
	
	// apply pre-exposure
	result *= asfloat(g_ExposureData.Load(0));

	PSOutput output;
	
	output.color = float4(result, 1.0);
	output.normalRoughness = float4(encodeOctahedron24(lightingParams.N), lightingParams.roughness);
	output.albedoMetalness = approximateLinearToSRGB(float4(lightingParams.albedo, lightingParams.metalness));
	
	return output;
}
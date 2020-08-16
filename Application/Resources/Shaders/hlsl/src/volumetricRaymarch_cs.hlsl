#include "bindingHelper.hlsli"
#include "volumetricRaymarch.hlsli"
#include "common.hlsli"
#include "commonVolumetricFog.hlsli"


RWTexture2D<float4> g_ResultImage : REGISTER_UAV(RESULT_IMAGE_BINDING, 0);
Texture2D<float> g_DepthImage : REGISTER_SRV(DEPTH_IMAGE_BINDING, 0);
ConstantBuffer<Constants> g_Constants : REGISTER_CBV(CONSTANT_BUFFER_BINDING, 0);
Texture2DArray<float4> g_ShadowImage : REGISTER_SRV(SHADOW_IMAGE_BINDING, 0);
StructuredBuffer<float4x4> g_ShadowMatrices : REGISTER_SRV(SHADOW_MATRICES_BINDING, 0);
ByteAddressBuffer g_ExposureData : REGISTER_SRV(EXPOSURE_DATA_BUFFER_BINDING, 0);
Texture2DArray<float4> g_BlueNoiseImage : REGISTER_SRV(BLUE_NOISE_IMAGE_BINDING, 0);

// media
StructuredBuffer<GlobalParticipatingMedium> g_GlobalMedia : REGISTER_SRV(GLOBAL_MEDIA_BINDING, 0);

// directional lights
StructuredBuffer<DirectionalLight> g_DirectionalLights : REGISTER_SRV(DIRECTIONAL_LIGHTS_BINDING, 0);
StructuredBuffer<DirectionalLight> g_DirectionalLightsShadowed : REGISTER_SRV(DIRECTIONAL_LIGHTS_SHADOWED_BINDING, 0);

SamplerState g_LinearSampler : REGISTER_SAMPLER(LINEAR_SAMPLER_BINDING, 0);
SamplerComparisonState g_ShadowSampler : REGISTER_SAMPLER(SHADOW_SAMPLER_BINDING, 0);

Texture3D<float4> g_Textures3D[TEXTURE_ARRAY_SIZE] : REGISTER_SRV(0, 1);

float4 scatterStep(float3 accumulatedLight, float accumulatedTransmittance, float3 sliceLight, float sliceExtinction, float stepLength)
{
	sliceExtinction = max(sliceExtinction, 1e-5);
	float sliceTransmittance = exp(-sliceExtinction * stepLength);
	
	float3 sliceLightIntegral = (-sliceLight * sliceTransmittance + sliceLight) * rcp(sliceExtinction);
	
	accumulatedLight += sliceLightIntegral * accumulatedTransmittance;
	accumulatedTransmittance *= sliceTransmittance;
	
	return float4(accumulatedLight, accumulatedTransmittance);
}

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
	const float shadow = tc.w != -1.0 ? g_ShadowImage.SampleCmpLevelZero(g_ShadowSampler, tc.xyw, tc.z) : 1.0;
	
	return shadow;
}

float henyeyGreenstein(float3 V, float3 L, float g)
{
	float num = -g * g + 1.0;
	float cosTheta = dot(-L, V);
	float denom = 4.0 * PI * pow((g * g + 1.0) - 2.0 * g * cosTheta, 1.5);
	return num / denom;
}

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	if (threadID.x >= g_Constants.width || threadID.y >= g_Constants.height)
	{
		return;
	}
	
	float2 uv = float2(threadID.xy + 0.5) * g_Constants.texelSize;
	
	float3 rayOrigin = lerp(g_Constants.frustumCornerTL, g_Constants.frustumCornerTR, uv.x);
	rayOrigin = lerp(rayOrigin, lerp(g_Constants.frustumCornerBL, g_Constants.frustumCornerBR, uv.x), uv.y);
	
	
	// world space ray dir
	const float3 rayDir = normalize(rayOrigin - g_Constants.cameraPos);
	
	//rayOrigin = g_Constants.cameraPos + rayDir * g_Constants.rayOriginDist;
	
	const float depth = g_DepthImage.Load(int3(threadID.xy, 0)).x;
	float2 clipSpacePosition = float2(uv * float2(2.0, -2.0) - float2(1.0, -1.0));
	float4 viewSpacePosition = float4(g_Constants.unprojectParams.xy * clipSpacePosition, -1.0, g_Constants.unprojectParams.z * depth + g_Constants.unprojectParams.w);
	float3 rayEndPosViewSpace = viewSpacePosition.xyz / viewSpacePosition.w;
	float3 rayOriginViewSpace = mul(g_Constants.viewMatrix, float4(rayOrigin, 1.0)).xyz;
	
	float rayLength = rayEndPosViewSpace.z < rayOriginViewSpace.z ? length(rayEndPosViewSpace - rayOriginViewSpace) : 0.0;
	
	const float stepCount = 16.0;
	const float stepSize = rayLength / stepCount;
	
	
	float3 viewSpaceV = mul(g_Constants.viewMatrix, float4(-rayDir, 0.0)).xyz;
	
	float4 accum = float4(0.0, 0.0, 0.0, 1.0);
	
	float noise = g_BlueNoiseImage.Load(int4((threadID.xy + 32 * (g_Constants.frame & 1)) & 63, g_Constants.frame & 63, 0)).x;
	
	for (float t = noise * stepSize; t < rayLength; t += stepSize)
	{
		float3 worldSpacePos = rayOrigin + rayDir * t;
		
		float3 scattering = 0.0;
		float extinction = 0.0;
		float3 emissive = 0.0;
		float phase = 0.0;
		
		// iterate over all global participating media
		{
			for (int i = 0; i < g_Constants.globalMediaCount; ++i)
			{
				GlobalParticipatingMedium medium = g_GlobalMedia[i];
				const float density = volumetricFogGetDensity(medium, worldSpacePos, g_Textures3D, g_LinearSampler);
				scattering += medium.scattering * density;
				extinction += medium.extinction * density;
				emissive += medium.emissive * density;
				phase += medium.phase;
			}
			phase = g_Constants.globalMediaCount > 0 ? phase * rcp((float)g_Constants.globalMediaCount) : 0.0;
		}
		
		
		const float4 scatteringExtinction = float4(scattering, extinction);
		const float4 emissivePhase = float4(emissive, phase);
		
		// integrate inscattered lighting
		float3 lighting = emissivePhase.rgb;
		{
			// ambient
			{
				float3 ambientLight = (1.0 / PI);
				lighting += ambientLight * (1.0 / (4.0 * PI));
			}
			
			// directional lights
			{
				for (uint i = 0; i < g_Constants.directionalLightCount; ++i)
				{
					DirectionalLight directionalLight = g_DirectionalLights[i];
					lighting += directionalLight.color * henyeyGreenstein(viewSpaceV, directionalLight.direction, emissivePhase.w);
				}
			}
			
			// shadowed directional lights
			{
				for (uint i = 0; i < g_Constants.directionalLightShadowedCount; ++i)
				{
					DirectionalLight directionalLight = g_DirectionalLightsShadowed[i];
					float shadow = getDirectionalLightShadow(directionalLight, worldSpacePos);
					lighting += directionalLight.color * henyeyGreenstein(viewSpaceV, directionalLight.direction, emissivePhase.w) * shadow;
				}
			}
		}
		
		float4 result = float4(lighting * scatteringExtinction.rgb, scatteringExtinction.w);
		
		accum = scatterStep(accum.rgb, accum.a, result.rgb, result.a, stepSize);
	}
	
	// apply pre-exposure
	accum.rgb *= asfloat(g_ExposureData.Load(0));
	
	g_ResultImage[threadID.xy] = accum;
}
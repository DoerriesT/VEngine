#include "bindingHelper.hlsli"
#include "ssrResolve.hlsli"
#include "srgb.hlsli"
#include "brdf.hlsli"
#include "common.hlsli"

RWTexture2D<float4> g_ResultImage : REGISTER_UAV(RESULT_IMAGE_BINDING, RESULT_IMAGE_SET);
RWTexture2D<float> g_ResultMaskImage : REGISTER_UAV(RESULT_MASK_IMAGE_BINDING, RESULT_MASK_IMAGE_SET);
Texture2D<float4> g_SpecularRoughnessImage : REGISTER_SRV(SPEC_ROUGHNESS_IMAGE_BINDING, SPEC_ROUGHNESS_IMAGE_SET);
Texture2D<float4> g_NormalImage : REGISTER_SRV(NORMAL_IMAGE_BINDING, NORMAL_IMAGE_SET);
Texture2D<float4> g_RayHitPdfImage : REGISTER_SRV(RAY_HIT_PDF_IMAGE_BINDING, RAY_HIT_PDF_IMAGE_SET);
Texture2D<float> g_MaskImage : REGISTER_SRV(MASK_IMAGE_BINDING, MASK_IMAGE_SET);
Texture2D<float> g_DepthImage : REGISTER_SRV(DEPTH_IMAGE_BINDING, DEPTH_IMAGE_SET);
Texture2D<float2> g_VelocityImage : REGISTER_SRV(VELOCITY_IMAGE_BINDING, VELOCITY_IMAGE_SET);
Texture2D<float4> g_PrevColorImage : REGISTER_SRV(PREV_COLOR_IMAGE_BINDING, PREV_COLOR_IMAGE_SET);
ConstantBuffer<Constants> g_Constants : REGISTER_CBV(CONSTANT_BUFFER_BINDING, CONSTANT_BUFFER_SET);
ByteAddressBuffer g_ExposureData : REGISTER_SRV(EXPOSURE_DATA_BUFFER_BINDING, EXPOSURE_DATA_BUFFER_SET);
SamplerState g_LinearSampler : REGISTER_SAMPLER(LINEAR_SAMPLER_BINDING, LINEAR_SAMPLER_SET);

float localBrdf(float3 N, float3 V, float3 L, float3 F0, float roughness)
{
	roughness = max(roughness, 1e-5);
	float a = roughness * roughness;
	float a2 = a * a;
	float3 H = normalize(V + L);
	float NdotV = saturate(dot(N, V));
	float NdotL = saturate(dot(N, L));
	float VdotH = saturate(dot(V, H));
	float NdotH = saturate(dot(N, H));
	return D_GGX(NdotH, a2) * V_SmithGGXCorrelated(NdotV, NdotL, a2) /* F_Schlick(F0, VdotH) */ * NdotL;
}

float ggxRoughnessToPhongSpecularExponent(float a)
{
	return 2.0 / (a * a) - 2.0;
}

float phongSpecularExponentToConeAngle(float s)
{
	return s <= 8192.0 ? acos(pow(0.244, 1.0 / (s + 1.0))) : 0.0;
}

float calculateMipLevel(float viewSpaceDepth, float hitDist, float roughness, float filterShrinkCompensation)
{
	if (roughness < 1e-5)
	{
		return 0.0;
	}
	float phongSpecExponent = ggxRoughnessToPhongSpecularExponent(roughness * roughness);
	float coneAngle = phongSpecularExponentToConeAngle(phongSpecExponent) * 0.5;
	
	float diameter = tan(coneAngle) * hitDist * 2.0;
	diameter *= filterShrinkCompensation;
	float diameterPixels = g_Constants.diameterToScreen * diameter / -viewSpaceDepth;
	
	float mipLevel = diameterPixels > 0.0 ? log2(diameterPixels) : 0.0;
	mipLevel *= g_Constants.bias;
	
	return mipLevel;
}

static int2 offsets[] = {int2(-1, -1), int2(2, -1), int2(-1, 2), int2(2, 2)};

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	//if (threadID.x >= g_Constants.width || threadID.y >= g_Constants.height)
	//{
	//	return;
	//}
	
	bool2 gap = bool2(g_Constants.horizontalGap, g_Constants.verticalGap);
	int2 coords[4];
	coords[0] = g_Constants.jitter + (gap ? ((threadID.xy / 3 * 6) + (threadID.xy % 3)) : threadID.xy * 2);
	coords[1] = coords[0] + (gap ? int2(3, 0) : int2(1, 0));
	coords[2] = coords[0] + (gap ? int2(0, 3) : int2(0, 1));
	coords[3] = coords[0] + (gap ? int2(3, 3) : int2(1, 1));
	
	float4 rayHitPdfValues[4];
	float4 taps[4];
	
	// load samples
	{
		const float exposureConversionFactor = asfloat(g_ExposureData.Load(1 << 2)); // 0 = current frame exposure | 1 = previous frame to current frame exposure
		
		for (int i = 0; i < 4; ++i)
		{
			rayHitPdfValues[i] = g_RayHitPdfImage.Load(int3(coords[i], 0));
			taps[i].a = g_MaskImage.Load(int3(coords[i], 0)).x;
			taps[i].rgb = 0.0;
			
			// reproject into last frame
			float2 reprojectedTexCoord = rayHitPdfValues[i].xy;
			float2 velocity = g_VelocityImage.SampleLevel(g_LinearSampler, reprojectedTexCoord, 0.0).xy;
			reprojectedTexCoord -= velocity;
			
			// is the uv coord still valid?
			taps[i].a *= all(reprojectedTexCoord > 0.0 && reprojectedTexCoord < 1.0) ? 1.0 : 0.0;
			
			if (g_Constants.ignoreHistory == 0 && taps[i].a > 0.0)
			{
				taps[i].rgb = g_PrevColorImage.SampleLevel(g_LinearSampler, reprojectedTexCoord, 0.0).rgb;
				// taps[i].rgb is pre-exposed -> convert from previous frame exposure to current frame exposure
				taps[i].rgb *= exposureConversionFactor;
			}
		}
	}
	
	// resolve all samples
	{
		for (int i = 0; i < 4; ++i)
		{
			const float depth = g_DepthImage.Load(int3(coords[i], 0)).x;
			
			// calculate view space position
			const float2 clipSpacePosition = float2(coords[i] + 0.5) * float2(g_Constants.texelWidth, g_Constants.texelHeight) * 2.0 - 1.0;
			float4 viewSpacePosition = float4(g_Constants.unprojectParams.xy * clipSpacePosition, -1.0, g_Constants.unprojectParams.z * depth + g_Constants.unprojectParams.w);
			viewSpacePosition.xyz /= viewSpacePosition.w;
			
			const float3 P = viewSpacePosition.xyz;
			const float3 V = -normalize(viewSpacePosition.xyz);
			const float4 specularRoughness = approximateSRGBToLinear(g_SpecularRoughnessImage.Load(int3(coords[i], 0)));
			const float3 F0 = specularRoughness.xyz;
			const float roughness = max(specularRoughness.w, 0.04); // avoid precision problems
			const float3 N = g_NormalImage.Load(int3(coords[i], 0)).xyz;
			
			const float filterShrinkCompensation = lerp(saturate(dot(N, V) * 2.0), 1.0, sqrt(roughness));
			
			float4 result = 0.0;
			float totalWeight = 0.0;
			float avgRayDepth = 0.0;
			float totalDepthWeight = 0.0;
			
			// iterate over all samples and weight their contribution to the current sample
			for (int j = 0; j < 4; ++j)
			{
				float4 viewSpaceHitPos = float4(g_Constants.unprojectParams.xy * (rayHitPdfValues[j].xy * 2.0 - 1.0), -1.0, g_Constants.unprojectParams.z * rayHitPdfValues[j].z + g_Constants.unprojectParams.w);
				viewSpaceHitPos.xyz /= viewSpaceHitPos.w;
				float3 L = (viewSpaceHitPos.xyz - P);
				float hitDist = length(L);
				hitDist = isnan(hitDist) ? 1e-7 : hitDist;
				L *= 1.0 / hitDist;
				
				float weight = localBrdf(N, V, L, F0, roughness) / max(rayHitPdfValues[j].w, 1e-6);
				weight *= taps[j].a > 0.0 ? 1.0 : 0.0;
				
				result += taps[j] * weight;
				totalWeight += weight;
				float rayDepthWeight = taps[j].a > 0.0 ? 1.0 : 0.0;
				avgRayDepth += viewSpaceHitPos.z * rayDepthWeight;
				totalDepthWeight += rayDepthWeight;
			}
			
			result *= 1.0 / max(totalWeight, float(1e-6));
			avgRayDepth *= 1.0 / max(totalDepthWeight, float(1e-6));
			
			// project to screen space depth
			avgRayDepth = (g_Constants.depthProjectParams.x * avgRayDepth + g_Constants.depthProjectParams.y) / -avgRayDepth;
			
			float resultMask = result.a;
			result.a = avgRayDepth;
			
			g_ResultImage[coords[i]] = result;
			g_ResultMaskImage[coords[i]] = resultMask;
		}
	}
}
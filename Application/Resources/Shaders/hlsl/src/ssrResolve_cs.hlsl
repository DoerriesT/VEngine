#include "bindingHelper.hlsli"
#include "ssrResolve.hlsli"
#include "srgb.hlsli"
#include "brdf.hlsli"
#include "common.hlsli"
#include "commonEncoding.hlsli"

RWTexture2D<float4> g_ResultImage : REGISTER_UAV(RESULT_IMAGE_BINDING, RESULT_IMAGE_SET);
RWTexture2D<float> g_ResultMaskImage : REGISTER_UAV(RESULT_MASK_IMAGE_BINDING, RESULT_MASK_IMAGE_SET);
Texture2D<float4> g_SpecularRoughnessImage : REGISTER_SRV(SPEC_ROUGHNESS_IMAGE_BINDING, SPEC_ROUGHNESS_IMAGE_SET);
Texture2D<float2> g_NormalImage : REGISTER_SRV(NORMAL_IMAGE_BINDING, NORMAL_IMAGE_SET);
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
	if (threadID.x >= g_Constants.width || threadID.y >= g_Constants.height)
	{
		return;
	}

	const float depth = g_DepthImage.Load(int3(threadID.xy, 0)).x;
	if (depth == 0.0)
	{
		g_ResultImage[threadID.xy] = 0.0;
		g_ResultMaskImage[threadID.xy] = 0.0;
		return;
	}
	
	const float2 clipSpacePosition = float2(threadID.xy + 0.5) * float2(g_Constants.texelWidth, g_Constants.texelHeight) * float2(2.0, -2.0) - float2(1.0, -1.0);
	float4 viewSpacePosition = float4(g_Constants.unprojectParams.xy * clipSpacePosition, -1.0, g_Constants.unprojectParams.z * depth + g_Constants.unprojectParams.w);
	viewSpacePosition.xyz /= viewSpacePosition.w;
	
	const float3 P = viewSpacePosition.xyz;
	const float3 V = -normalize(viewSpacePosition.xyz);
	const float4 specularRoughness = approximateSRGBToLinear(g_SpecularRoughnessImage.Load(int3(threadID.xy, 0)));
	const float3 F0 = specularRoughness.xyz;
	const float roughness = max(specularRoughness.w, 0.04); // avoid precision problems
	const float3 N = decodeOctahedron(g_NormalImage.Load(int3(threadID.xy, 0)).xy);
	
	const float filterShrinkCompensation = lerp(saturate(dot(N, V) * 2.0), 1.0, sqrt(max(roughness, 1e-7)));
	
	const float exposureConversionFactor = asfloat(g_ExposureData.Load(1 << 2)); // 0 = current frame exposure | 1 = previous frame to current frame exposure
	
	// process rays
	float totalWeight = 0.0;
	float4 result = 0.0;
	float avgRayDepth = 0.0;
	float totalDepthWeight = 0.0;
	for (int i = 0; i < 4; ++i)
	{
		int2 coord = offsets[i] + int2(threadID.xy);
		float4 rayHitPdf = g_RayHitPdfImage.Load(int3(coord / 2, 0));
		float mask = g_MaskImage.Load(int3(coord / 2, 0)).x;
		
		float4 viewSpaceHitPos = float4(g_Constants.unprojectParams.xy * (rayHitPdf.xy * float2(2.0, -2.0) - float2(1.0, -1.0)), -1.0, g_Constants.unprojectParams.z * rayHitPdf.z + g_Constants.unprojectParams.w);
		viewSpaceHitPos.xyz /= viewSpaceHitPos.w;
		float3 L = (viewSpaceHitPos.xyz - P);
		float hitDist = length(L);
		hitDist = isnan(hitDist) ? 1e-7 : hitDist;
		L *= 1.0 / hitDist;
		
		float weight = localBrdf(N, V, L, F0, roughness) / max(rayHitPdf.w, 1e-6);
		weight *= mask > 0.0 ? 1.0 : 0.0;
		float4 sampleColor = float4(0.0, 0.0, 0.0, mask);
		if (g_Constants.ignoreHistory == 0 && sampleColor.a > 0.0)
		{
			// reproject into last frame
			float2 velocity = g_VelocityImage.SampleLevel(g_LinearSampler, rayHitPdf.xy, 0.0).xy;
			rayHitPdf.xy -= velocity;
			
			// is the uv coord still valid?
			sampleColor.a *= (rayHitPdf.x > 0.0 && rayHitPdf.y > 0.0 && rayHitPdf.x < 1.0 && rayHitPdf.y < 1.0) ? 1.0 : 0.0;
			
			float mipLevel = calculateMipLevel(P.z, hitDist, roughness, filterShrinkCompensation);
			sampleColor.rgb = sampleColor.a > 0.0 ? g_PrevColorImage.SampleLevel(g_LinearSampler, rayHitPdf.xy, mipLevel).rgb : 0.0;
			
			// sampleColor.rgb is pre-exposed -> convert from previous frame exposure to current frame exposure
			sampleColor.rgb *= exposureConversionFactor;
		}
		
		result += sampleColor * weight;// vec4(sampleColor.rgb * weight, sampleColor.a);
		totalWeight += weight;
		float rayDepthWeight = mask > 0.0 ? 1.0 : 0.0;
		avgRayDepth += viewSpaceHitPos.z * rayDepthWeight;
		totalDepthWeight += rayDepthWeight;
	}
	
	result *= 1.0 / max(totalWeight, float(1e-6));
	avgRayDepth *= 1.0 / max(totalDepthWeight, float(1e-6));
	
	// project to screen space depth
	avgRayDepth = (g_Constants.depthProjectParams.x * avgRayDepth + g_Constants.depthProjectParams.y) / -avgRayDepth;
	
	float resultMask = result.a;
	result.a = avgRayDepth;
	
	g_ResultImage[threadID.xy] = result;
	g_ResultMaskImage[threadID.xy] = resultMask;
}
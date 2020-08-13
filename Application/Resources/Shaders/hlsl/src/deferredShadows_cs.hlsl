#include "bindingHelper.hlsli"
#include "deferredShadows.hlsli"
#include "commonFourierOpacity.hlsli"

ConstantBuffer<Constants> g_Constants : REGISTER_CBV(CONSTANT_BUFFER_BINDING, 0);
RWTexture2D<float> g_ResultImage : REGISTER_UAV(RESULT_IMAGE_BINDING, 0);
Texture2D<float> g_DepthImage : REGISTER_SRV(DEPTH_IMAGE_BINDING, 0);
Texture2DArray<float> g_ShadowImage : REGISTER_SRV(SHADOW_IMAGE_BINDING, 0);
SamplerComparisonState g_ShadowSampler : REGISTER_SAMPLER(SHADOW_SAMPLER_BINDING, 0);
SamplerState g_PointSampler : REGISTER_SAMPLER(POINT_SAMPLER_BINDING, 0);
SamplerState g_LinearSampler : REGISTER_SAMPLER(LINEAR_SAMPLER_BINDING, 0);
StructuredBuffer<float4x4> g_ShadowMatrices : REGISTER_SRV(SHADOW_MATRICES_BINDING, 0);
StructuredBuffer<float4> g_CascadeParams : REGISTER_SRV(CASCADE_PARAMS_BUFFER_BINDING, 0);  // X: depth bias Y: normal bias Z: texelsPerMeter
Texture2DArray<float4> g_BlueNoiseImage : REGISTER_SRV(BLUE_NOISE_IMAGE_BINDING, 0);
Texture2DArray<float4> g_FomImage : REGISTER_SRV(FOM_IMAGE_BINDING, 0);


//PUSH_CONSTS(PushConsts, g_PushConsts);

float interleavedGradientNoise(float2 v)
{
	float3 magic = float3(0.06711056, 0.00583715, 52.9829189);
	return frac(magic.z * dot(v, magic.xy));
}

float2 vogelDiskSample(uint sampleIndex, uint samplesCount, float phi)
{
	const float goldenAngle = 2.4;
	const float r = sqrt(sampleIndex + 0.5) / sqrt(samplesCount);
	const float theta = sampleIndex * goldenAngle + phi;

	float2 result;
	sincos(theta, result.y, result.x);
	return result * r;
}

float avgBlockersDepthToPenumbra(float zShadowMapView, float avgBlockersDepth)
{
	float penumbra = (zShadowMapView - avgBlockersDepth) / avgBlockersDepth;
	penumbra *= penumbra;
	return saturate(4000.0 * penumbra);
}

float penumbra(float noise, float3 shadowCoord, int samplesCount, float layer)
{
	float avgBlockersDepth = 0.0;
	float blockersCount = 0.0;
	
	for (int i = 0; i < samplesCount; ++i)
	{
		const float penumbraFilterMaxSize = 7.0 * (1.0 / 2048.0);
		float2 sampleCoord = vogelDiskSample(i, samplesCount, noise);
		sampleCoord = shadowCoord.xy + penumbraFilterMaxSize * sampleCoord;
		
		float sampleDepth = g_ShadowImage.SampleLevel(g_PointSampler, float3(sampleCoord, layer), 0.0).x;
	
		if (sampleDepth < shadowCoord.z)
		{
			avgBlockersDepth += sampleDepth;
			blockersCount += 1.0;
		}
	}
	
	if (blockersCount > 0.0)
	{
		avgBlockersDepth /= blockersCount;
		return avgBlockersDepthToPenumbra(shadowCoord.z, avgBlockersDepth);
	}
	else
	{
		return 0.0;
	}
}

float3 minDiff(float3 P, float3 Pr, float3 Pl)
{
    float3 V1 = Pr - P;
    float3 V2 = P - Pl;
    return (dot(V1, V1) < dot(V2, V2)) ? V1 : V2;
}

float3 getViewSpacePosDepth(float2 texelCoord, float depth)
{
	const float2 clipSpacePosition = texelCoord * float2(g_Constants.texelWidth, g_Constants.texelHeight) * float2(2.0, -2.0) - float2(1.0, -1.0);
	float4 viewSpacePosition = float4(g_Constants.unprojectParams.xy * clipSpacePosition, -1.0, g_Constants.unprojectParams.z * depth + g_Constants.unprojectParams.w);
	return viewSpacePosition.xyz / viewSpacePosition.w;
}

float3 getViewSpacePos(float2 texelCoord)
{
	return getViewSpacePosDepth(texelCoord, g_DepthImage.Load(int3(texelCoord, 0)).x);
}

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
		g_ResultImage[threadID.xy] = 1.0;
		return;
	}
	
	float3 P = getViewSpacePosDepth(float2(threadID.xy + 0.5), depth);
	
	// Sample neighboring pixels
	const float3 Pr = getViewSpacePos(float2(threadID.xy) + 0.5 + float2(1.0, 0.0));
	const float3 Pl = getViewSpacePos(float2(threadID.xy) + 0.5 + float2(-1.0, 0.0));
	const float3 Pb = getViewSpacePos(float2(threadID.xy) + 0.5 + float2(0.0, 1.0));
	const float3 Pt = getViewSpacePos(float2(threadID.xy) + 0.5 + float2(0.0, -1.0));
	
	// Calculate tangent basis vectors using the minimum difference
	const float3 dPdu = minDiff(P, Pr, Pl);
	const float3 dPdv = minDiff(P, Pt, Pb);
		
	const float3 viewSpaceNormal = normalize(cross(dPdu, dPdv));
	
	const float3 N = mul((float3x3)g_Constants.invViewMatrix, viewSpaceNormal);
	
	const float NdotL = dot(viewSpaceNormal.xyz, g_Constants.direction.xyz);
		
	// skip backfacing fragments
	if (NdotL <= -0.05)
	{
		g_ResultImage[threadID.xy] = 0.0;
		return;
	}
	
	const float scaleWeight = 1.0 - max(NdotL, 0.0);
	
	float4 worldSpacePos = mul(g_Constants.invViewMatrix, float4(P, 1.0));
	worldSpacePos *= 1.0 / worldSpacePos.w;
	
	float4 tc = -1.0;
	const int cascadeDataOffset = g_Constants.cascadeDataOffset;
	const float maxFilterRadiusWorldSpace = 0.04;
	uint3 imageDims;
	g_ShadowImage.GetDimensions(imageDims.x, imageDims.y, imageDims.z);
	const float texelSize = 1.0 / imageDims.x;
	for (int i = g_Constants.cascadeCount - 1; i >= 0; --i)
	{
		const float normalOffsetScale = g_CascadeParams[cascadeDataOffset + i].y * scaleWeight;
		const float4 shadowCoord = float4(mul(g_ShadowMatrices[cascadeDataOffset + i], float4(worldSpacePos.xyz + N * normalOffsetScale, 1.0)).xyz, cascadeDataOffset + i);
		const float border = maxFilterRadiusWorldSpace * g_CascadeParams[cascadeDataOffset + i].z * texelSize;
		const bool valid = all(abs(shadowCoord.xy) < (border * -2.0 + 1.0));
		tc = valid ? shadowCoord : tc;
	}
	
	tc.xy = tc.xy * float2(0.5, -0.5) + 0.5;
	tc.z += g_CascadeParams[int(tc.w)].x;

	float shadow = 0.0;
	const float noise = g_BlueNoiseImage.Load(int4((threadID.xy + 32 * (g_Constants.frame & 1)) & 63, g_Constants.frame & 63, 0)).x * 2.0 * 3.1415;
	//const float noise = interleavedGradientNoise(float2(threadID.xy) + 0.5) * 2.0 * 3.1415;
	const float filterScale = penumbra(noise, tc.xyz, 16, tc.w);
	const float maxFilterRadiusTexelSpace = maxFilterRadiusWorldSpace * g_CascadeParams[int(tc.w)].z * texelSize;
	for (uint j = 0; j < 16; ++j)
	{
		const float2 coord = filterScale * maxFilterRadiusTexelSpace * vogelDiskSample(j, 16, noise) + tc.xy;
		shadow += g_ShadowImage.SampleCmpLevelZero(g_ShadowSampler, float3(coord, tc.w), tc.z).x * (1.0 / 16.0);
	}
	
	//if (shadow > 0.0)
	{
		float4 fom0 = g_FomImage.SampleLevel(g_LinearSampler, float3(tc.xy, tc.w * 2.0 + 0.0), 0.0);
		float4 fom1 = g_FomImage.SampleLevel(g_LinearSampler, float3(tc.xy, tc.w * 2.0 + 1.0), 0.0);
		
		shadow *= fourierOpacityGetTransmittance(tc.z, fom0, fom1);
	}
	
	g_ResultImage[threadID.xy] = shadow;
}
#include "bindingHelper.hlsli"
#include "fourierOpacityVolume.hlsli"
#include "common.hlsli"
#include "commonEncoding.hlsli"
#include "commonFourierOpacity.hlsli"
#include "commonVolumetricFog.hlsli"

StructuredBuffer<LightInfo> g_LightInfo : REGISTER_SRV(LIGHT_INFO_BINDING, 0);
StructuredBuffer<GlobalParticipatingMedium> g_GlobalMedia : REGISTER_SRV(GLOBAL_MEDIA_BINDING, 0);
SamplerState g_LinearSampler : REGISTER_SAMPLER(LINEAR_SAMPLER_BINDING, 0);

Texture3D<float4> g_Textures3D[TEXTURE_ARRAY_SIZE] : REGISTER_SRV(0, 1);

PUSH_CONSTS(PushConsts, g_PushConsts);

struct PSInput
{
	float4 position : SV_Position;
	float2 texCoord : TEXCOORD;
};

struct PSOutput
{
	float4 fom0 : SV_Target0;
	float4 fom1 : SV_Target1;
};

PSOutput main(PSInput input)
{
	const LightInfo lightInfo = g_LightInfo[g_PushConsts.lightIndex];

	// compute ray from light source passing through the current pixel
	float3 ray = 0.0;
	if (lightInfo.isPointLight)
	{
		ray = -decodeOctahedron(input.texCoord * 2.0 - 1.0);
	}
	else
	{
		const float4 farPlaneWorldSpacePos = mul(lightInfo.invViewProjection, float4(input.texCoord * float2(2.0, -2.0) - float2(1.0, -1.0), 1.0, 1.0));
		ray = normalize((farPlaneWorldSpacePos.xyz / farPlaneWorldSpacePos.w) - lightInfo.position);
	}
								
	float t0 = 0.0;
	float t1 = lightInfo.radius;
	
	float segmentLength = t1 - t0;
	
	float4 result0 = 0.0;
	float4 result1 = 0.0;
	
	// march ray
	//const float stepSize = 0.1;
	//const int stepCount = (int)ceil((rayEnd - rayStart) / stepSize);
	const int stepCount = clamp(ceil(segmentLength / 0.05), 1, 64);
	const float stepSize = segmentLength / stepCount;
	
	//float2x2 ditherMatrix = float2x2(0.25, 0.75, 1.0, 0.5);
	//float dither = ditherMatrix[groupID.y % 2][groupID.x % 2] * stepSize;
	
	float4x4 ditherMatrix = float4x4(1 / 16.0, 9 / 16.0, 3 / 16.0, 11 / 16.0,
									13 / 16.0, 5 / 16.0, 15 / 16.0, 7 / 16.0,
									4 / 16.0, 12 / 16.0, 2 / 16.0, 10 / 16.0,
									16 / 16.0, 8 / 16.0, 14 / 16.0, 6 / 16.0);
	float dither = ditherMatrix[(int)input.position.y % 4][(int)input.position.x % 4] * stepSize;
	
	t0 += dither + stepSize * 0.5;
	float invRadius = rcp(lightInfo.radius);
	
	for (int i = 0; i < stepCount; ++i)
	{
		float t = i * stepSize + t0;
		float3 rayPos = lightInfo.position + ray * t;
		
		float extinction = 0.0;
		
		// add extinction of all global volumes
		{
			for (int j = 0; j < g_PushConsts.globalMediaCount; ++j)
			{
				GlobalParticipatingMedium medium = g_GlobalMedia[j];
				const float density = volumetricFogGetDensity(medium, rayPos, g_Textures3D, g_LinearSampler);
				extinction += medium.extinction * density;
			}
		}
		
		float transmittance = exp(-extinction * stepSize);
		float depth = t * invRadius;
		fourierOpacityAccumulate(depth, transmittance, result0, result1);
	}
	
	
	PSOutput output;
	output.fom0 = result0;
	output.fom1 = result1;
	
	return output;
}
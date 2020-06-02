#include "bindingHelper.hlsli"
#include "fomVolume.hlsli"
#include "common.hlsli"

struct PSInput
{
	float4 position : SV_Position;
	float3 ray : RAY;
};

struct PSOutput
{
	float4 result0 : SV_Target0;
	float4 result1 : SV_Target1;
};

StructuredBuffer<LightInfo> g_LightInfo : REGISTER_SRV(LIGHT_INFO_BINDING, 0);
StructuredBuffer<VolumeInfo> g_VolumeInfo : REGISTER_SRV(VOLUME_INFO_BINDING, 0);

PUSH_CONSTS(PushConsts, g_PushConsts);

float getExtinction(float3 position)
{
	return g_VolumeInfo[g_PushConsts.volumeIndex].extinction;
}

float interleavedGradientNoise(float2 v)
{
    float3 magic = float3(0.06711056, 0.00583715, 52.9829189);
    return frac(magic.z * dot(v, magic.xy));
}

PSOutput main(PSInput input)
{
	LightInfo lightInfo = g_LightInfo[g_PushConsts.lightIndex];
	VolumeInfo volumeInfo = g_VolumeInfo[g_PushConsts.volumeIndex];
	
	PSOutput output = (PSOutput)0;
	
	// ray is in world space
	float3 ray = normalize(input.ray);
	
	float3 localRay;
	localRay.x = dot(volumeInfo.worldToLocal0, float4(ray, 0.0));
	localRay.y = dot(volumeInfo.worldToLocal1, float4(ray, 0.0));
	localRay.z = dot(volumeInfo.worldToLocal2, float4(ray, 0.0));
	
	float3 localOrigin;
	localOrigin.x = dot(volumeInfo.worldToLocal0, float4(lightInfo.position, 1.0));
	localOrigin.y = dot(volumeInfo.worldToLocal1, float4(lightInfo.position, 1.0));
	localOrigin.z = dot(volumeInfo.worldToLocal2, float4(lightInfo.position, 1.0));
	
	
	// find intersections with volume
	float t0;
	float t1;
	bool hit = true;
	
	// aabb intersection
	{
		float tMin = 0.0;
		float tMax = 9999999.0;
		for (int a = 0; a < 3; ++a)
		{
			float invD = 1.0f / localRay[a];
			float t0tmp = (-1.0f - localOrigin[a]) * invD;
			float t1tmp = (1.0f - localOrigin[a]) * invD;
			if (invD < 0.0f)
			{
				float tmp = t0tmp;
				t0tmp = t1tmp;
				t1tmp = tmp;
			}
			tMin = t0tmp > tMin ? t0tmp : tMin;
			tMax = t1tmp < tMax ? t1tmp : tMax;
	
			if (tMax <= tMin)
			{
				hit = false;
				break;
			}
		}
		
		t0 = tMin;
		t1 = tMax;
		
		
		//float3 invDir = rcp(localRay);
		//float3 origin = lightInfo.position;
		//float3 localOrigin;
		//localOrigin.x = dot(volumeInfo.worldToLocal0, float4(origin, 1.0));
		//localOrigin.y = dot(volumeInfo.worldToLocal1, float4(origin, 1.0));
		//localOrigin.z = dot(volumeInfo.worldToLocal2, float4(origin, 1.0));
		//float3 originDivDir = origin * invDir;
		//const float3 tMin = -1.0 * invDir - originDivDir;
		//const float3 tMax = 1.0 * invDir - originDivDir;
		//const float3 tNear = min(tMin, tMax);
		//const float3 tFar = max(tMin, tMax);
		//t0 = max(max(tNear.x, tNear.y), max(tNear.z, 0.0));
		//t1 = min(min(tFar.x, tFar.y), tFar.z);
	}
	
	if (!hit)
	{
		return output;
	}
	
	const float stepSize = 0.1;
	int stepCount = (int)ceil((t1 - t0) / stepSize);
	
	t0 += (interleavedGradientNoise(input.position.xy) - 0.5) * stepSize;
	
	// raymarch through volume
	for (int i = 0; i < stepCount; ++i)
	{
		float t = i * stepSize + t0;
		float3 rayPos = ray * t;
		
		float extinction = getExtinction(rayPos);
		float transmittance = exp(-extinction * stepSize);
		float depth = t * lightInfo.depthScale + lightInfo.depthBias;
		
		//float lnTransmittance = -log(transmittance);
		//
		//output.result0.r = 0.f;
		//
		//// DC component
		//output.result0.g = lnTransmittance;
		//
		//// These constants need to be applied as the result of integration
		//const float recipPi = 1.f/PI;
		//const float halfRecipPi = recipPi * 0.5f;
		//const float thirdRecipPi = recipPi/3.f;
		//const float quarterRecipPi = recipPi * 0.25f;
		//const float fifthRecipPi = recipPi * 0.2f;
		//const float sixthRecipPi = recipPi/6.f;
		//const float seventhRecipPi = recipPi/7.f;
		//
		//// Remaining outputs require sin/cos
		//float2 cs1;
		//sincos(PI * 2.0 * depth, cs1.y, cs1.x);
		//output.result0.ba = recipPi * lnTransmittance * cs1;
		//
		//float2 csn = float2(cs1.x*cs1.x-cs1.y*cs1.y, 2.f*cs1.y*cs1.x);
		//output.result1.rg = halfRecipPi * lnTransmittance * csn;
		//
		//csn = float2(csn.x*cs1.x-csn.y*cs1.y, csn.y*cs1.x+csn.x*cs1.y);
		//output.result1.ba = thirdRecipPi * lnTransmittance * csn;
		
		output.result0.r += -2.0f * log(transmittance) * cos(2.0 * PI * 0.0 * depth);
		output.result0.g += -2.0f * log(transmittance) * sin(2.0 * PI * 0.0 * depth);
		
		output.result0.b += -2.0f * log(transmittance) * cos(2.0 * PI * 1.0 * depth);
		output.result0.a += -2.0f * log(transmittance) * sin(2.0 * PI * 1.0 * depth);
		
		output.result1.r += -2.0f * log(transmittance) * cos(2.0 * PI * 2.0 * depth);
		output.result1.g += -2.0f * log(transmittance) * sin(2.0 * PI * 2.0 * depth);
		
		output.result1.b += -2.0f * log(transmittance) * cos(2.0 * PI * 3.0 * depth);
		output.result1.a += -2.0f * log(transmittance) * sin(2.0 * PI * 3.0 * depth);
	}
	
	return output;
}
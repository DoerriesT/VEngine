#include "bindingHelper.hlsli"
#include "fourierOpacityVolumeDirectional.hlsli"
#include "common.hlsli"
#include "commonFourierOpacity.hlsli"
#include "commonVolumetricFog.hlsli"

StructuredBuffer<LocalParticipatingMedium> g_LocalMedia : REGISTER_SRV(LOCAL_MEDIA_BINDING, 0);
Texture2DArray<float> g_DepthRangeImage : REGISTER_SRV(DEPTH_RANGE_IMAGE_BINDING, 0);

Texture3D<float4> g_Textures3D[TEXTURE_ARRAY_SIZE] : REGISTER_SRV(0, 1);

SamplerState g_Samplers[SAMPLER_COUNT] : REGISTER_SAMPLER(0, 2);

PUSH_CONSTS(PushConsts, g_PushConsts);

struct PSInput
{
	float4 position : SV_Position;
};

struct PSOutput
{
	float4 fom0 : SV_Target0;
	float4 fom1 : SV_Target1;
};

PSOutput main(PSInput input)
{
	const LocalParticipatingMedium medium = g_LocalMedia[g_PushConsts.volumeIndex];

	// compute ray from light source passing through the current pixel
	float3 ray = g_PushConsts.rayDir;
	float3 rayOrigin = g_PushConsts.topLeft + input.position.x * g_PushConsts.deltaX + input.position.y * g_PushConsts.deltaY;
	
	
	// compute intersections with volume
	const float3 localRay = float3(dot(medium.worldToLocal0, float4(ray, 0.0)), 
								dot(medium.worldToLocal1, float4(ray, 0.0)), 
								dot(medium.worldToLocal2, float4(ray, 0.0)));
								
	const float3 localLightPos = float3(dot(medium.worldToLocal0, float4(rayOrigin, 1.0)), 
								dot(medium.worldToLocal1, float4(rayOrigin, 1.0)), 
								dot(medium.worldToLocal2, float4(rayOrigin, 1.0)));
								
	float t0;
	float t1;
	
	// intersect aabb
	if (medium.spherical == 0)
	{
		float3 invD = rcp(localRay);
		float3 t0s = (-1.0 - localLightPos) * invD;
		float3 t1s = (1.0 - localLightPos) * invD;
		float3 tmin = min(t0s, t1s);
		float3 tmax = max(t0s, t1s);
		
		t0 = max(tmin.x, max(tmin.y, tmin.z));
		t1 = min(tmax.x, min(tmax.y, tmax.z));
	}
	// intersect sphere
	else
	{
		float a = dot(localRay, localRay);
		float b = 2.0 * dot(localRay, localLightPos);
		float c = dot(localLightPos, localLightPos) - 1.0;
		
		// solve quadratic equation
		{
			float discr = b * b - 4.0 * a * c;
			if (discr < 0.0)
			{
				// no intersection
				t0 = t1 = -1.0;
			}
			else if (discr == 0.0)
			{
				t0 = t1 = -0.5 * b / a;
			}
			else
			{
				float q = (b > 0.0) ? -0.5 * (b + sqrt(discr)) : -0.5 * (b - sqrt(discr));
				t0 = q / a;
				t1 = c / q;
				
				if (t0 > t1)
				{
					float tmp = t0;
					t0 = t1;
					t1 = tmp;
				}
			}
		}
	}
	
	// no intersection
	if (t0 < 0.0 || t0 > t1)
	{
		discard;
	}
	
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
	
	float rangeBegin = g_DepthRangeImage.Load(uint4(input.position.xy, g_PushConsts.shadowMatrixIndex * 2, 0)).x;
	float rangeEnd = g_DepthRangeImage.Load(uint4(input.position.xy, g_PushConsts.shadowMatrixIndex * 2 + 1, 0)).x;
	
	for (int i = 0; i < stepCount; ++i)
	{
		float t = i * stepSize + t0;
		float3 rayPos = rayOrigin + ray * t;
		
		float extinction = 0.0;
		
		const float3 localPos = float3(dot(medium.worldToLocal0, float4(rayPos, 1.0)), 
								dot(medium.worldToLocal1, float4(rayPos, 1.0)), 
								dot(medium.worldToLocal2, float4(rayPos, 1.0)));
								
		if (all(abs(localPos) <= 1.0) && (medium.spherical == 0 || dot(localPos, localPos) <= 1.0))
		{
			float density = volumetricFogGetDensity(medium, localPos, g_Textures3D, g_Samplers[SAMPLER_LINEAR_CLAMP]);
			extinction = medium.extinction * density;
		}
		
		float transmittance = exp(-extinction * stepSize);
		float depth = t * g_PushConsts.invLightRange;
		
		depth = clamp(depth, rangeBegin, rangeEnd);
		depth = (depth - rangeBegin) / (rangeEnd - rangeBegin);
		
		fourierOpacityAccumulate(depth, transmittance, result0, result1);
	}
	
	
	PSOutput output;
	output.fom0 = result0;
	output.fom1 = result1;
	
	return output;
}
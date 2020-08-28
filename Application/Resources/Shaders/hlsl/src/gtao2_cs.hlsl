#include "bindingHelper.hlsli"
#include "gtao2.hlsli"
#include "common.hlsli"
#include "commonEncoding.hlsli"

RWTexture2D<float2> g_ResultImage : REGISTER_UAV(RESULT_IMAGE_BINDING, 0);
Texture2D<float> g_DepthImage : REGISTER_SRV(DEPTH_IMAGE_BINDING, 0);
Texture2D<float4> g_NormalImage : REGISTER_SRV(NORMAL_IMAGE_BINDING, 0);
ConstantBuffer<Constants> g_Constants : REGISTER_CBV(CONSTANT_BUFFER_BINDING, 0);

SamplerState g_Samplers[SAMPLER_COUNT] : REGISTER_SAMPLER(0, 1);

//PUSH_CONSTS(PushConsts, g_PushConsts);

float3 getViewSpacePos(float2 uv)
{
	float depth = g_DepthImage.SampleLevel(g_Samplers[SAMPLER_POINT_CLAMP], uv, 0.0).x;
	float2 clipSpacePosition = float2(uv * float2(2.0, -2.0) - float2(1.0, -1.0));
	float4 viewSpacePosition = float4(g_Constants.unprojectParams.xy * clipSpacePosition, -1.0, g_Constants.unprojectParams.z * depth + g_Constants.unprojectParams.w);
	return viewSpacePosition.xyz / viewSpacePosition.w;
}

float3 minDiff(float3 P, float3 Pr, float3 Pl)
{
    float3 V1 = Pr - P;
    float3 V2 = P - Pl;
    return (dot(V1, V1) < dot(V2, V2)) ? V1 : V2;
}

float fastSqrt(float x)
{
	// [Drobot2014a] Low Level Optimizations for GCN
	return asfloat(0x1FBD1DF5 + (asint(x) >> 1));
}

float fastAcos(float x)
{
	// [Eberly2014] GPGPU Programming for Games and Science
	float res = -0.156583 * abs(x) + PI / 2.0;
	res *= fastSqrt(1.0 - abs(x));
	return x >= 0 ? res : PI - res;
}

// x = spatial direction / y = temporal direction / z = spatial offset / w = temporal offset
float4 getNoise(int2 coord, int frame)
{
	float4 noise;
	
	noise.x = (1.0 / 16.0) * ((((coord.x + coord.y) & 0x3 ) << 2) + (coord.x & 0x3));
	noise.z = (1.0 / 4.0) * ((coord.y - coord.x) & 0x3);

	const float rotations[] = { 60.0, 300.0, 180.0, 240.0, 120.0, 0.0 };
	noise.y = rotations[frame % 6] * (1.0 / 360.0);
	
	const float offsets[] = { 0.0, 0.5, 0.25, 0.75 };
	noise.w = offsets[(frame / 6 ) % 4];
	
	return noise;
}

float getHorizonCosAngle(float3 centerPos, float3 samplePos, float3 V)
{
	float3 sampleDir = samplePos - centerPos;
	float dist = length(sampleDir);
	sampleDir = normalize(sampleDir);
	
	float falloff = saturate(dist * g_Constants.falloffScale + g_Constants.falloffBias);
	return lerp(dot(sampleDir, V), -1.0, falloff);
}

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	if (threadID.x >= g_Constants.width || threadID.y >= g_Constants.height)
	{
		return;
	}

	float2 texCoord = (threadID.xy + 0.5) * g_Constants.texelSize;
	float3 centerPos = getViewSpacePos(texCoord);
	float3 V = normalize(-centerPos);
	
	const float3 N = mul((float3x3)g_Constants.viewMatrix, decodeOctahedron24(g_NormalImage.Load(int3(threadID.xy, 0)).xyz));
	
	//// Sample neighboring pixels
	//const float3 Pr = getViewSpacePos((float2(threadID.xy + 0.5) + float2(1.0, 0.0)) * g_Constants.texelSize);
	//const float3 Pl = getViewSpacePos((float2(threadID.xy + 0.5) + float2(-1.0, 0.0)) * g_Constants.texelSize);
	//const float3 Pb = getViewSpacePos((float2(threadID.xy + 0.5) + float2(0.0, 1.0)) * g_Constants.texelSize);
	//const float3 Pt = getViewSpacePos((float2(threadID.xy + 0.5) + float2(0.0, -1.0)) * g_Constants.texelSize);
	//
	//// Calculate tangent basis vectors using the minimum difference
	//const float3 dPdu = minDiff(centerPos, Pr, Pl);
	//const float3 dPdv = minDiff(centerPos, Pt, Pb);
	//
	//const float3 N = normalize(cross(dPdu, dPdv));
	
	float scaling = g_Constants.radiusScale / -centerPos.z;
	scaling = min(scaling, g_Constants.texelSize.x * g_Constants.maxTexelRadius);
	int sampleCount = min(ceil(scaling * g_Constants.width), g_Constants.sampleCount);
	
	float visibility = 1.0;
	
	if (sampleCount > 0)
	{
		visibility = 0.0;
		float4 noise = getNoise(threadID.xy, g_Constants.frame);

		float theta = (noise.x + noise.y) * PI;
		float jitter = noise.z + noise.w;
		float3 dir = 0.0;
		sincos(theta, dir.y, dir.x);
		
		float3 orthoDir = dir - dot(dir, V) * V;
		float3 axis = cross(dir, V);
		float3 projNormal = N - axis * dot(N, axis);
		float projNormalLen = length(projNormal);
		
		float sgnN = sign(dot(orthoDir, projNormal));
		float cosN = saturate(dot(projNormal, V) / projNormalLen);
		float n = sgnN * fastAcos(cosN);
		float sinN = sin(n);
		
		[unroll]
		for (int side = 0; side < 2; ++side)
		{
			float horizonCos = -1.0;
			
			for (int sample = 0; sample < sampleCount; ++sample)
			{
				float s = (sample + jitter) / (float)sampleCount;
				s *= s;
				float2 sampleTexCoord = texCoord + (-1.0 + 2.0 * side) * s * scaling * float2(dir.x, -dir.y);
				float3 samplePos = getViewSpacePos(sampleTexCoord);
				float sampleHorizonCos = getHorizonCosAngle(centerPos, samplePos, V);
				
				const float beta = 0.05;
				const float minDistToMax = 0.1;
				horizonCos = ((horizonCos - sampleHorizonCos) > minDistToMax) ? max(horizonCos - beta, -1.0) : max(sampleHorizonCos, horizonCos);
			}	
			
			float h = n + clamp((-1.0 + 2.0 * side) * fastAcos(horizonCos) - n, -PI / 2, PI / 2);
			visibility += projNormalLen * (cosN + 2.0 * h * sinN - cos(2.0 * h - n)) * 0.25;
		}
	}
	
	visibility = saturate(visibility);

	g_ResultImage[threadID.xy] = float2(visibility, -centerPos.z);
}
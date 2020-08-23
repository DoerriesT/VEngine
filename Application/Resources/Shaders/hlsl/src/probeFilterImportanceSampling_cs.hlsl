#include "bindingHelper.hlsli"
#include "probeFilterImportanceSampling.hlsli"
#include "monteCarlo.hlsli"
#include "common.hlsli"

RWTexture2DArray<float4> g_ResultImages[7] : REGISTER_UAV(RESULT_IMAGE_BINDING, 0);
TextureCube<float4> g_InputImage : REGISTER_SRV(INPUT_IMAGE_BINDING, 0);

SamplerState g_Samplers[SAMPLER_COUNT] : REGISTER_SAMPLER(0, 1);

PUSH_CONSTS(PushConsts, g_PushConsts);

static int sampleCount = 32;

// http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
// efficient VanDerCorpus calculation.
float RadicalInverse_VdC(uint bits) 
{
     bits = (bits << 16u) | (bits >> 16u);
     bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
     bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
     bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
     bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
     return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

float2 Hammersley(uint i, uint N)
{
	return float2(float(i)/float(N), RadicalInverse_VdC(i));
}

float3 integrateCubeLDOnly(float3 N, float roughness)
{
	float a2 = roughness * roughness;
	a2 *= a2;
	float3 accBrdf = 0.0;
	float accBrdfWeight = 0.0;
	
	for (int i = 0; i < sampleCount; ++i)
	{
		float2 eta = Hammersley(i, sampleCount);
		float3 H = importanceSampleGGX(eta, N, a2);
		float3 V = N;
		float3 L = reflect(-V, H);
		float NdotL = dot(N, L);
		
		if (NdotL > 0.0)
		{
			float NdotH = saturate(dot(N, H));
			float LdotH = saturate(dot(L, H));
			float pdf = (D_GGX(NdotH, a2) / PI) * NdotH / (4.0 * LdotH);
			// solid angle of sample
			float omegaS = 1.0 / (sampleCount * pdf) + 1e-5;
			// solid angle of cubemap pixel
			float omegaP = 4.0 * PI / (6.0 * g_PushConsts.width * g_PushConsts.width);
			float mipLevel = clamp(0.5 * log2(omegaS / omegaP), 0.0, g_PushConsts.mipCount);
			float3 Li = g_InputImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], L, mipLevel).rgb;
			
			accBrdf += Li * NdotL;
			accBrdfWeight += NdotL;
		}
	}
	
	return accBrdf * (1.0 / accBrdfWeight);
}

float3 getDir(float2 texCoord, int face)
{
	texCoord = texCoord * 2.0 - 1.0;
	texCoord.y = -texCoord.y;

	switch (face)
	{
	case 0:
		return float3(1.0, texCoord.y, -texCoord.x);
	case 1:
		return float3(-1.0, texCoord.y, texCoord.x);
	case 2:
		return float3(texCoord.x, 1.0, -texCoord.y);
	case 3:
		return float3(texCoord.x, -1.0, texCoord.y);
	case 4:
		return float3(texCoord.x, texCoord.y, 1.0);
	default:
		return float3(-texCoord.x, texCoord.y, -1.0);
	}
}

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	if (threadID.x >= g_PushConsts.width || threadID.y >= g_PushConsts.width)
	{
		return;
	}
	
	float2 texCoord = (threadID.xy + 0.5) * g_PushConsts.texelSize;
	float3 N = getDir(texCoord, threadID.z);
	
	float3 result;
	if (g_PushConsts.mip == 0)
	{
		result = g_InputImage.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], N, 0.0).rgb;
	}
	else
	{
		result = integrateCubeLDOnly(N, g_PushConsts.roughness);
	}
	
	g_ResultImages[g_PushConsts.mip][threadID] = float4(result, 1.0);
}
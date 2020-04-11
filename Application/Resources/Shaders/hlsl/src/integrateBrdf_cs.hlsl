#include "bindingHelper.hlsli"
#include "integrateBrdf.hlsli"
#include "monteCarlo.hlsli"

RWTexture2D<float2> g_ResultImage : REGISTER_UAV(RESULT_IMAGE_BINDING, RESULT_IMAGE_SET);

PUSH_CONSTS(PushConsts, g_PushConsts);

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

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	if (threadID.x >= g_PushConsts.width || threadID.y >= g_PushConsts.height)
	{
		return;
	}
	
	float roughness = (threadID.x + 0.5) * g_PushConsts.texelWidth;
	float NdotV = (threadID.y + 0.5) * g_PushConsts.texelHeight;
	float a = roughness * roughness;
	float a2 = a * a;
	
	float3 V;
	V.x = sqrt(1.0 - NdotV * NdotV); // sin
	V.y = 0.0;
	V.z = NdotV; // cos;
	
	float A = 0.0;
	float B = 0.0;
	
	const uint numSamples = 1024;
	for (uint i = 0; i < numSamples; ++i)
	{
		float2 Xi = Hammersley(i, numSamples);
		float3 H = importanceSampleGGX(Xi, float3(0.0, 0.0, 1.0), a2);
		float3 L = 2.0 * dot(V, H) * H - V;
		
		float NdotL = saturate(L.z);
		float NdotH = saturate(H.z);
		float VDotH = saturate(dot(V, H));
		
		if (NdotL > 0.0)
		{
			float Vis = V_SmithGGXCorrelated(NdotV, NdotL, a2);
			float NdotL_Vis_PDF = NdotL * Vis * 4.0 * VDotH / NdotH;

			float Fc = pow5(1.0 - VDotH);
			A += (1.0 - Fc) * NdotL_Vis_PDF;
			B += Fc * NdotL_Vis_PDF;
		}
	}
	
	g_ResultImage[threadID.xy] = float2(A, B) * (1.0 / numSamples);
}
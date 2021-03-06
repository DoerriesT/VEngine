#include "bindingHelper.hlsli"
#include "sharpen_ffx_cas.hlsli"
#include "srgb.hlsli"
#include "dither.hlsli"

RWTexture2D<float4> g_ResultImage : REGISTER_UAV(RESULT_IMAGE_BINDING, 0);
Texture2D<float4> g_InputImage : REGISTER_SRV(INPUT_IMAGE_BINDING, 0);

PUSH_CONSTS(PushConsts, g_PushConsts);

#define A_GPU 1
#define A_HLSL 1
#include "ffx_a.h"

AF3 CasLoad( ASU2 p )
{
	return g_InputImage.Load(int3(p, 0)).rgb;
}

void CasInput(inout AF1 r, inout AF1 g, inout AF1 b) 
{
	if (g_PushConsts.gammaSpaceInput != 0)
	{
		float3 c = float3(r, g, b);
		c = accurateSRGBToLinear(c);
		r = c.r;
		g = c.g;
		b = c.b;
	}
}

#include "ffx_cas.h"


float4 gammaCorrectAndDither(float3 c, uint2 coord)
{
	c = accurateLinearToSRGB(c);
	c = ditherTriangleNoise(c, (coord + 0.5) * g_PushConsts.texelSize, g_PushConsts.time);
	
	return float4(c, 1.0);
}

[numthreads(64, 1, 1)]
void main(uint3 groupThreadID : SV_GroupThreadID, uint3 groupID : SV_GroupID)
{
	AU2 gxy = ARmp8x8(groupThreadID.x) + AU2(groupID.x << 4u, groupID.y << 4u);
	
	const uint4 const0 = g_PushConsts.const0;
	const uint4 const1 = g_PushConsts.const1;
	
	AF3 c;
	CasFilter(c.r, c.g, c.b, gxy, const0, const1, true);
	g_ResultImage[gxy] = gammaCorrectAndDither(c, gxy);
	gxy.x += 8u;
	
	CasFilter(c.r, c.g, c.b, gxy, const0, const1, true);
	g_ResultImage[gxy] = gammaCorrectAndDither(c, gxy);
	gxy.y += 8u;
	
	CasFilter(c.r, c.g, c.b, gxy, const0, const1, true);
	g_ResultImage[gxy] = gammaCorrectAndDither(c, gxy);
	gxy.x -= 8u;
	
	CasFilter(c.r, c.g, c.b, gxy, const0, const1, true);
	g_ResultImage[gxy] = gammaCorrectAndDither(c, gxy);
}
#include "bindingHelper.hlsli"

RWTexture2D<float4> g_ResultTexture : REGISTER_UAV(0, 0);
Texture2D<float4> g_InputTexture : REGISTER_SRV(1, 0);

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	g_ResultTexture[threadID.xy] = float4(g_InputTexture.Load(int3(threadID.xy, 0)).rgb, 1.0);
}
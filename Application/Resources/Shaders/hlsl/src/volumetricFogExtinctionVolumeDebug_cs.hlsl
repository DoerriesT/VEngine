#include "bindingHelper.hlsli"
#include "volumetricFogExtinctionVolumeDebug.hlsli"

RWTexture2D<float4> g_ResultImage : REGISTER_UAV(RESULT_IMAGE_BINDING, 0);
Texture3D<float> g_ExtinctionImage : REGISTER_SRV(EXTINCTION_IMAGE_BINDING, 0);

PUSH_CONSTS(PushConsts, g_PushConsts);

float raymarch(const float3 origin, const float3 dir)
{
	// grid space
	float3 gridCoord = origin * g_PushConsts.coordScale + g_PushConsts.coordBias;
	const float3 gridStep = sign(dir);
	const float3 corner = max(gridStep, 0.0);
	
	// ray space
	const float3 inv = 1.0 / dir;
	float3 ratio = (floor(gridCoord) + corner - gridCoord) * inv;
	const float3 ratioStep = gridStep * inv;
	gridCoord = floor(gridCoord);
	
	float3 dims;
	g_ExtinctionImage.GetDimensions(dims.x, dims.y, dims.z);
	
	float extinctionSum = 0.0;
	
	while (all(gridCoord >= 0.0) && all(gridCoord < dims))
	{
		extinctionSum += g_ExtinctionImage.Load(int4(gridCoord, 0)).x;
		
		const int3 mask = int3(ratio.xyz <= min(ratio.yzx, ratio.zxy));
		gridCoord += gridStep * mask;		
		ratio += ratioStep * mask;
	}
	
	return extinctionSum;
}

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	float2 resultDims;
	g_ResultImage.GetDimensions(resultDims.x, resultDims.y);
	float2 resultTexelSize = 1.0 / resultDims;
	
	float4 worldSpacePos = mul(g_PushConsts.invViewProjection, float4((threadID.xy + 0.5) * resultTexelSize * float2(2.0, -2.0) - float2(1.0, -1.0), 1.0, 1.0));
	worldSpacePos.xyz /= worldSpacePos.w;
	
	float4 direction4 = mul(g_PushConsts.invViewProjection, float4((threadID.xy + 0.5) * resultTexelSize * float2(2.0, -2.0) - float2(1.0, -1.0), 0.0, 1.0));
	float3 direction = normalize(direction4.xyz / direction4.w - worldSpacePos.xyz);

	float extinction = raymarch(worldSpacePos.xyz, direction);
	
	float4 resultColor = lerp(float4(0.0, 0.0, 1.0, 0.0), float4(0.0, 0.0, 0.0, 1.0), saturate(extinction * 0.05));
	
	g_ResultImage[threadID.xy] = float4(lerp(g_ResultImage[threadID.xy].rgb, resultColor.rgb, resultColor.a), 1.0);
}
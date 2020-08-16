#include "bindingHelper.hlsli"
#include "volumetricFogIntegrate.hlsli"
#include "commonFilter.hlsli"

#define VOLUME_DEPTH (64)
#define VOLUME_NEAR (0.5)
#define VOLUME_FAR (64.0)

ConstantBuffer<Constants> g_Constants : REGISTER_CBV(CONSTANT_BUFFER_BINDING, 0);
RWTexture3D<float4> g_ResultImage : REGISTER_UAV(RESULT_IMAGE_BINDING, 0);
Texture3D<float4> g_InputImage : REGISTER_SRV(INPUT_IMAGE_BINDING, 0);


float getStepLength(int slice)
{
	float logFarOverNear = log2(VOLUME_FAR / VOLUME_NEAR);
	float d0 = slice * (1.0 / VOLUME_DEPTH);
	float d1 = (slice + 1.0) * (1.0 / VOLUME_DEPTH);
	
	return VOLUME_NEAR * (exp2(d1 * logFarOverNear) - exp2(d0 * logFarOverNear));
}

float3 calcWorldSpacePos(float3 texelCoord)
{
	uint3 imageDims;
	g_ResultImage.GetDimensions(imageDims.x, imageDims.y, imageDims.z);
	float2 uv = texelCoord.xy / float2(imageDims.xy);
	
	float3 pos = lerp(g_Constants.frustumCornerTL, g_Constants.frustumCornerTR, uv.x);
	pos = lerp(pos, lerp(g_Constants.frustumCornerBL, g_Constants.frustumCornerBR, uv.x), uv.y);
	
	//pos = normalize(pos);
	
	float d = texelCoord.z * (1.0 / VOLUME_DEPTH);
	float z = VOLUME_NEAR * exp2(d * (log2(VOLUME_FAR / VOLUME_NEAR)));
	pos *= z / VOLUME_FAR;
	
	pos += g_Constants.cameraPos;
	
	return pos;
}

float4 scatterStep(float3 accumulatedLight, float accumulatedTransmittance, float3 sliceLight, float sliceExtinction, float stepLength)
{
	sliceExtinction = max(sliceExtinction, 1e-5);
	float sliceTransmittance = exp(-sliceExtinction * stepLength);
	
	float3 sliceLightIntegral = (-sliceLight * sliceTransmittance + sliceLight) * rcp(sliceExtinction);
	
	accumulatedLight += sliceLightIntegral * accumulatedTransmittance;
	accumulatedTransmittance *= sliceTransmittance;
	
	return float4(accumulatedLight, accumulatedTransmittance);
}

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	float4 accum = float4(0.0, 0.0, 0.0, 1.0);
	
	float3 prevWorldSpacePos = calcWorldSpacePos(float3(threadID.xy + 0.5, 0.0));
	
	for (int z = 0; z < VOLUME_DEPTH; ++z)
	{
		float3 worldSpacePos = calcWorldSpacePos(float3(threadID.xy + 0.5, z + 1.0));
		float stepLength = distance(prevWorldSpacePos, worldSpacePos);
		prevWorldSpacePos = worldSpacePos;
		int4 pos = int4(threadID.xy, z, 0);
		float4 slice = g_InputImage.Load(pos);
		accum = scatterStep(accum.rgb, accum.a, slice.rgb, slice.a, stepLength);
		float4 result = accum;
		result.rgb = simpleTonemap(result.rgb);
		g_ResultImage[pos.xyz] = result;
	}
}
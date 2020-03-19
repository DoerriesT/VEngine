#include "bindingHelper.hlsli"
#include "volumetricFogIntegrate.hlsli"

#define VOLUME_DEPTH (64)
#define VOLUME_NEAR (0.5)
#define VOLUME_FAR (64.0)

RWTexture3D<float4> g_ResultImage : REGISTER_UAV(RESULT_IMAGE_BINDING, RESULT_IMAGE_SET);
Texture3D<float4> g_InputImage : REGISTER_SRV(INPUT_IMAGE_BINDING, INPUT_IMAGE_SET);


float getStepLength(int slice)
{
	float logFarOverNear = log2(VOLUME_FAR / VOLUME_NEAR);
	float d0 = slice * (1.0 / VOLUME_DEPTH);
	float d1 = (slice + 1.0) * (1.0 / VOLUME_DEPTH);
	
	return VOLUME_NEAR * (exp2(d1 * logFarOverNear) - exp2(d0 * logFarOverNear));
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
	
	for (int z = 0; z < VOLUME_DEPTH; ++z)
	{
		int4 pos = int4(threadID.xy, z, 0);
		float4 slice = g_InputImage.Load(pos);
		accum = scatterStep(accum.rgb, accum.a, slice.rgb, slice.a, getStepLength(z));
		g_ResultImage[pos.xyz] = accum;
	}
}
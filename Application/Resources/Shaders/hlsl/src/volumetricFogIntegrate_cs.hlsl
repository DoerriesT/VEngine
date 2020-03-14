#include "bindingHelper.hlsli"
#include "volumetricFogIntegrate.hlsli"

#define VOLUME_DEPTH (64)

RWTexture3D<float4> g_ResultImage : REGISTER_UAV(RESULT_IMAGE_BINDING, RESULT_IMAGE_SET);
Texture3D<float4> g_InputImage : REGISTER_SRV(INPUT_IMAGE_BINDING, INPUT_IMAGE_SET);

float4 scatterStep(float3 accumulatedLight, float accumulatedTransmittance, float3 sliceLight, float sliceDensity)
{
	sliceDensity = max(sliceDensity, 1e-5);
	float sliceTransmittance = exp(-sliceDensity / VOLUME_DEPTH);
	
	float3 sliceLightIntegral = sliceLight * (1.0 - sliceTransmittance) / sliceDensity;
	
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
		accum = scatterStep(accum.rgb, accum.a, slice.rgb, slice.a);
		g_ResultImage[pos.xyz] = accum;
	}
}
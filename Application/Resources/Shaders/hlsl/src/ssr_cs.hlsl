#include "bindingHelper.hlsli"
#include "ssr.hlsli"
#include "srgb.hlsli"
#include "monteCarlo.hlsli"
#include "common.hlsli"
#include "commonEncoding.hlsli"

RWTexture2D<float4> g_RayHitPdfImage : REGISTER_UAV(RAY_HIT_PDF_IMAGE_BINDING, RAY_HIT_PDF_IMAGE_SET);
RWTexture2D<float> g_MaskImage : REGISTER_UAV(MASK_IMAGE_BINDING, MASK_IMAGE_SET);
Texture2D<float4> g_NormalRoughnessImage : REGISTER_SRV(NORMAL_ROUGHNESS_IMAGE_BINDING, NORMAL_ROUGHNESS_IMAGE_SET);
Texture2D<float> g_HiZPyramidImage : REGISTER_SRV(HIZ_PYRAMID_IMAGE_BINDING, HIZ_PYRAMID_IMAGE_SET);
ConstantBuffer<Constants> g_Constants : REGISTER_CBV(CONSTANT_BUFFER_BINDING, CONSTANT_BUFFER_SET);

Texture2D<float4> g_Textures[TEXTURE_ARRAY_SIZE] : REGISTER_SRV(TEXTURES_BINDING, TEXTURES_SET);
SamplerState g_Samplers[SAMPLER_COUNT] : REGISTER_SAMPLER(SAMPLERS_BINDING, SAMPLERS_SET);

float2 getCell(float2 ray, float2 cellCount)
{
	return floor(ray.xy * cellCount);
}

float2 getCellCount(float level)
{
	float2 texSize;
	g_HiZPyramidImage.GetDimensions(texSize.x, texSize.y);
	return texSize / (level == 0.0 ? 1.0 : exp2(level));
}

float3 intersectCellBoundary(float3 pos, float3 dir, float2 cellIdx, float2 cellCount, float2 crossStep, float2 crossOffset)
{
	float2 cellSize = 1.0 / cellCount;
	float2 planes = (cellIdx + crossStep) * cellSize;
	
	float2 solutions = (planes - pos.xy) / dir.xy;
	float3 intersectionPos = pos + dir * min(solutions.x, solutions.y);
	intersectionPos.xy += (solutions.x < solutions.y) ? float2(crossOffset.x, 0.0) : float2(0.0, crossOffset.y);
	
	return intersectionPos;
}

bool crossedCellBoundary(float2 cellIdx0, float2 cellIdx1)
{
	return (int(cellIdx0.x) != int(cellIdx1.x)) || (int(cellIdx0.y) != int(cellIdx1.y));
}

float getMinimumDepthPlane(float2 ray, float level, float2 cellCount)
{
	return g_HiZPyramidImage.Load(int3(int2(ray.xy * cellCount), level)).x;
}

float3 hiZTrace(float3 p, float3 v)
{
	float level = 1.0;
	float iterations = 0.0;
	float3 vZ = v / v.z;
	vZ = -vZ;
	float2 hiZSize = getCellCount(level);
	float3 ray = p;
	
	float2 crossStep = float2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0);
	float2 crossOffset = crossStep * 0.00001;
	crossStep = saturate(crossStep);
	
	float2 rayCell = getCell(ray.xy, hiZSize);
	ray = intersectCellBoundary(ray, v, rayCell, hiZSize, crossStep, crossOffset);
	
	const float MAX_ITERATIONS = 1024.0;
	float minZ = 0.0;
	
	while(level >= 0.0 && iterations < MAX_ITERATIONS)
	{
		const float2 cellCount = getCellCount(level);
		const float2 oldCellIdx = getCell(ray.xy, cellCount);
		
		
		minZ = getMinimumDepthPlane(ray.xy, level, cellCount);
		
		float3 tmpRay = ray;
		if (v.z < 0.0)
		{
			float minMinusRay = (1.0 - minZ) - (1.0 - ray.z);
			tmpRay = minMinusRay > 0.0 ? ray + vZ * minMinusRay : tmpRay;
			const float2 newCellIdx = getCell(tmpRay.xy, cellCount);
			
			if (crossedCellBoundary(oldCellIdx, newCellIdx))
			{
				tmpRay = intersectCellBoundary(ray, v, oldCellIdx, cellCount, crossStep, crossOffset);
				level = min(g_Constants.hiZMaxLevel, level + 2.0);
			}
		}
		else if (ray.z > minZ)
		{
			tmpRay = intersectCellBoundary(ray, v, oldCellIdx, cellCount, crossStep, crossOffset);
			level = min(g_Constants.hiZMaxLevel, level + 2.0);
		}
		

		ray = tmpRay;
		
		if (ray.x < 0.0 || ray.y < 0.0 || ray.x > 1.0 || ray.y > 1.0)
		{
			return -1.0;
		}
		
		--level;
		++iterations;
	}
	return iterations >= MAX_ITERATIONS || (minZ - ray.z) >= 0.0003 ? -1.0 : ray;
}

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	if (threadID.x / 2 >= g_Constants.width || threadID.y / 2 >= g_Constants.height)
	{
		return;
	}
	
	const int2 fullResCoord = int2(threadID.xy) * 2 + g_Constants.subsample;

	const float depth = g_HiZPyramidImage.Load(int3(fullResCoord, 0)).x;
	if (depth == 0.0)
	{
		g_RayHitPdfImage[threadID.xy] = float4(-1.0, -1.0, -1.0, 1.0);
		g_MaskImage[threadID.xy] = 0.0;
		return;
	}
	
	const float2 clipSpacePosition = float2(fullResCoord + 0.5) * float2(g_Constants.texelWidth, g_Constants.texelHeight) * float2(2.0, -2.0) - float2(1.0, -1.0);
	float4 viewSpacePosition = float4(g_Constants.unprojectParams.xy * clipSpacePosition, -1.0, g_Constants.unprojectParams.z * depth + g_Constants.unprojectParams.w);
	viewSpacePosition.xyz /= viewSpacePosition.w;
	
	const float3 P = viewSpacePosition.xyz;
	const float3 V = -normalize(viewSpacePosition.xyz);
	float4 normalRoughness = g_NormalRoughnessImage.Load(int3(fullResCoord, 0));
	const float3 N = decodeOctahedron24(normalRoughness.xyz);
	const float roughness = max(normalRoughness.w, 0.04); // avoid precision problems
	
	float3 H = N;
	float3 R = N;
	float pdf = 1.0;
	float a = roughness * roughness;
	float a2 = a * a;
	for (int i = 0; i < 8; ++i)
	{
		float2 noiseTexCoord = float2(threadID.xy + 0.5) * g_Constants.noiseScale + (g_Constants.noiseJitter) * (i + 1);
		float2 noise = g_Textures[g_Constants.noiseTexId].SampleLevel(g_Samplers[SAMPLER_POINT_REPEAT], noiseTexCoord, 0.0).xy;
		noise.y = lerp(noise.y, 0.0, g_Constants.bias);
		H = importanceSampleGGX(noise, N, a2);
		R = reflect(-V, H.xyz);
		if (dot(N, R) > 1e-7)
		{
			pdf = pdfGGX(N, V, H, a2);
			break;
		}
	}
	
	float3 pSS = float3(float2(fullResCoord + 0.5) * float2(g_Constants.texelWidth, g_Constants.texelHeight), depth);
	float4 reflectSS = mul(g_Constants.projectionMatrix, float4(P + R, 1.0));
	reflectSS.xyz /= reflectSS.w;
	reflectSS.xy = reflectSS.xy * float2(0.5, -0.5) + 0.5;
	float3 vSS = reflectSS.xyz - pSS;
	
	float4 rayHitPdf = float4(hiZTrace(pSS, vSS), pdf);
	g_RayHitPdfImage[threadID.xy] = rayHitPdf;
	
	float2 uvSamplingAttenuation = smoothstep(0.05, 0.1, rayHitPdf.xy) * (1.0 - smoothstep(0.95, 1.0, rayHitPdf.xy));
	float mask = saturate(uvSamplingAttenuation.x * uvSamplingAttenuation.y);
	mask *= (rayHitPdf.x > 0.0 && rayHitPdf.y > 0.0 && rayHitPdf.x < 1.0 && rayHitPdf.y < 1.0) ? 1.0 : 0.0;
	mask *= dot(N, R) > 1e-7 ? 1.0 : 0.0;
	g_MaskImage[threadID.xy] = mask;
}
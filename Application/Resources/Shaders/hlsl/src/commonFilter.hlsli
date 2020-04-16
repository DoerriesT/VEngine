#ifndef FILTER_H
#define FILTER_H

// based on https://developer.nvidia.com/gpugems/gpugems2/part-iii-high-quality-rendering/chapter-20-fast-third-order-texture-filtering
float bSplineW0(float a)
{
	return (1.0 / 6.0) * (a * (a * (-a + 3.0) - 3.0) + 1.0);
}

float bSplineW1(float a)
{
	return (1.0 / 6.0) * (a * a * (3.0 * a - 6.0) + 4.0);
}

float bSplineW2(float a)
{
	return (1.0 / 6.0) * (a * (a * (-3.0 * a + 3.0) + 3.0) + 1.0);
}

float bSplineW3(float a)
{
	return (1.0 / 6.0) * (a * a * a);
}

// weight functions
float bSplineG0(float a)
{
	return bSplineW0(a) + bSplineW1(a);
}

float bSplineG1(float a)
{
	return bSplineW2(a) + bSplineW3(a);
}

// offset functions
float bSplineH0(float a)
{
	return -1.0 + bSplineW1(a) / (bSplineW0(a) + bSplineW1(a));
}

float bSplineH1(float a)
{
	return 1.0 + bSplineW3(a) / (bSplineW2(a) + bSplineW3(a));
}

void calculateBicubicSampleParams(float2 tc, float2 texSize, float2 texelSize, out float2 t[2], out float2 g[2])
{
	tc = tc * texSize + 0.5;
	float2 f = frac(tc);
	float2 fi = tc - f;
	
	float2 w0 = float2(bSplineW0(f.x), bSplineW0(f.y));
	float2 w1 = float2(bSplineW1(f.x), bSplineW1(f.y));
	float2 w3 = float2(bSplineW3(f.x), bSplineW3(f.y));
	float2 w2 = float2(bSplineW2(f.x), bSplineW2(f.y));
	
	g[0] = w0 + w1;
	g[1] = w2 + w3;
	
	float2 h0 = -1.0 + w1 / (w0 + w1);
	float2 h1 = 1.0 + w3 / (w2 + w3);
	
	t[0] = (fi + h0 - 0.5) * texelSize;
	t[1] = (fi + h1 - 0.5) * texelSize;
}

float4 sampleBicubic(Texture3D<float4> tex, SamplerState linearSampler, float2 tc, float2 texSize, float2 texelSize, float d)
{
	float2 t[2];
	float2 g[2];
	calculateBicubicSampleParams(tc, texSize, texelSize, t, g);
	
	return tex.SampleLevel(linearSampler, float3(t[0].x, t[0].y, d), 0.0) * g[0].x * g[0].y
		+ tex.SampleLevel(linearSampler, float3(t[1].x, t[0].y, d), 0.0) * g[1].x * g[0].y
		+ tex.SampleLevel(linearSampler, float3(t[0].x, t[1].y, d), 0.0) * g[0].x * g[1].y
		+ tex.SampleLevel(linearSampler, float3(t[1].x, t[1].y, d), 0.0) * g[1].x * g[1].y;
}

float3 simpleTonemap(float3 color)
{
	float luma = dot(color, float3(0.2126, 0.7152, 0.0722));
	return color / max(1.0 + luma, 1e-6);
}

float3 inverseSimpleTonemap(float3 color)
{
	float luma = dot(color, float3(0.2126, 0.7152, 0.0722));
	return color / max(1.0 - luma, 1e-6);
}


void neighborHoodVarianceAABB(Texture2D<float4> inputImage, int2 coord, float wideningFactor, out float4 minColor, out float4 maxColor, out float4 centerColor)
{
	float4 m1 = 0.0;
	float4 m2 = 0.0;
	for (int y = 0; y < 3; ++y)
	{
		for (int x = 0; x < 3; ++x)
		{
			float4 tap = inputImage.Load(int3(coord + int2(x, y) - 1, 0));
			
			m1 += tap;
			m2 += tap * tap;
			centerColor = x == 1 && y == 1 ? tap : centerColor;
		}
	}	
	
	float4 mean = m1 * (1.0 / 9.0);
	float4 stddev = sqrt(max((m2  * (1.0 / 9.0) - mean * mean), 1e-7));
	
	minColor = -stddev * wideningFactor + mean;
	maxColor = stddev * wideningFactor + mean;
}

float3 clipAABB(float3 p, float3 aabbMin, float3 aabbMax)
{
    //Clips towards AABB center for better perfomance
    float3 center   = 0.5 * (aabbMax + aabbMin);
    float3 halfSize = 0.5 * (aabbMax - aabbMin) + 1e-5;
    //Relative position from the center
    float3 clip     = p - center;
    //Normalize relative position
    float3 unit     = clip / halfSize;
    float3 absUnit  = abs(unit);
    float maxUnit = max(absUnit.x, max(absUnit.y, absUnit.z));
	
	return (maxUnit > 1.0) ? clip * (1.0 / maxUnit) + center : p;
}

float2 weightedLerpFactors(float weightA, float weightB, float blend)
{
	float blendA = (1.0 - blend) * weightA;
	float blendB = blend * weightB;
	float invBlend = 1.0 / (blendA + blendB);
	blendA *= invBlend;
	blendB *= invBlend;
	return float2(blendA, blendB);
}

#endif // FILTER_H
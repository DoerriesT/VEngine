#include "bindingHelper.hlsli"
#include "gtao.hlsli"

RWTexture2D<float2> g_ResultImage : REGISTER_UAV(RESULT_IMAGE_BINDING, RESULT_IMAGE_SET);
Texture2D<float> g_DepthImage : REGISTER_SRV(DEPTH_IMAGE_BINDING, DEPTH_IMAGE_SET);
SamplerState g_PointSampler : REGISTER_SAMPLER(POINT_SAMPLER_BINDING, POINT_SAMPLER_SET);

PUSH_CONSTS(PushConsts, g_PushConsts);

static const float PI = 3.14159265;

float3 getViewSpacePos(float2 uv)
{
	uv *= g_PushConsts.resolution.zw;
	float depth = g_DepthImage.SampleLevel(g_PointSampler, uv, 0.0).x;
	float4 clipSpacePosition = float4(uv * 2.0 - 1.0, depth, 1.0);
	float4 viewSpacePosition = mul(g_PushConsts.invProjection, clipSpacePosition);
	viewSpacePosition /= viewSpacePosition.w;
	//viewSpacePosition.z = -viewSpacePosition.z;
	return viewSpacePosition.xyz;
}

float3 minDiff(float3 P, float3 Pr, float3 Pl)
{
    float3 V1 = Pr - P;
    float3 V2 = P - Pl;
    return (dot(V1, V1) < dot(V2, V2)) ? V1 : V2;
}

void computeSteps(inout float stepSizePix, inout float numSteps, float rayRadiusPix)
{
    // Avoid oversampling if numSteps is greater than the kernel radius in pixels
    numSteps = min(g_PushConsts.numSteps, rayRadiusPix);
    stepSizePix = rayRadiusPix / numSteps;
}

// x = spatial direction / y = temporal direction / z = spatial offset / w = temporal offset
float4 getNoise(int2 coord)
{
	float4 noise;
	
	noise.x = (1.0 / 16.0) * ((((coord.x + coord.y) & 0x3 ) << 2) + (coord.x & 0x3));
	noise.z = (1.0 / 4.0) * ((coord.y - coord.x) & 0x3);

	const float rotations[] = { 60.0, 300.0, 180.0, 240.0, 120.0, 0.0 };
	noise.y = rotations[g_PushConsts.frame % 6] * (1.0 / 360.0);
	
	const float offsets[] = { 0.0, 0.5, 0.25, 0.75 };
	noise.w = offsets[(g_PushConsts.frame / 6 ) % 4];
	
	return noise;
}

float square(float x)
{
	return x * x;
}

float falloff(float dist2)
{
	float start = square(g_PushConsts.radius * 0.2);
	float end = square(g_PushConsts.radius);
	return 2.0 * clamp((dist2 - start) / (end - start), 0.0, 1.0);
}

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	if (threadID.x >= g_PushConsts.resolution.x || threadID.y >= g_PushConsts.resolution.y)
	{
		return;
	}

	float3 P = getViewSpacePos(float2(threadID.xy + 0.5));

	// Calculate the projected size of the hemisphere
    float rayRadiusUV = 0.5 * g_PushConsts.radius * g_PushConsts.focalLength / -P.z;
    float rayRadiusPix = rayRadiusUV * g_PushConsts.resolution.x;
	rayRadiusPix = min(rayRadiusPix, g_PushConsts.maxRadiusPixels);

    float ao = 1.0;

    // Make sure the radius of the evaluated hemisphere is more than a pixel
    if(rayRadiusPix > 1.0)
    {
		ao = 0.0;
		
		const float3 V = -normalize(P);
		
		float numSteps;
    	float stepSizePix;

    	// Compute the number of steps
    	computeSteps(stepSizePix, numSteps, rayRadiusPix);

		float4 noise = getNoise(threadID.xy);

		float theta = (noise.x + noise.y) * PI;
		float jitter = noise.z + noise.w;
		float2 dir;
		sincos(theta, dir.y, dir.x);
		float2 horizons = -1.0;
		
		float currstep = frac(jitter) * (stepSizePix - 1.0) + 1.0;

		for(float s = 0; s < numSteps; ++s)
		{
			float2 offset = currstep * dir;
			currstep += stepSizePix;
			
			// first horizon
			{
				float3 S = getViewSpacePos(float2(threadID.xy + 0.5) + offset);
				float3 D = S - P;
				float dist2 = dot(D, D);
				D *= rsqrt(dist2);
				float attenuation = falloff(dist2);
				horizons.x = max(dot(V, D) - attenuation, horizons.x);
			}
			
			// second horizon
			{
				float3 S = getViewSpacePos(float2(threadID.xy + 0.5) - offset);
				float3 D = S - P;
				float dist2 = dot(D, D);
				D *= rsqrt(dist2);
				float attenuation = falloff(dist2);
				horizons.y = max(dot(V, D) - attenuation, horizons.y);
			}
		}
		
		horizons = acos(horizons);
		horizons.x = -horizons.x;
		
		// Sample neighboring pixels
		const float3 Pr = getViewSpacePos(float2(threadID.xy + 0.5) + float2(1.0, 0.0));
		const float3 Pl = getViewSpacePos(float2(threadID.xy + 0.5) + float2(-1.0, 0.0));
		const float3 Pb = getViewSpacePos(float2(threadID.xy + 0.5) + float2(0.0, 1.0));
		const float3 Pt = getViewSpacePos(float2(threadID.xy + 0.5) + float2(0.0, -1.0));
		
		// Calculate tangent basis vectors using the minimum difference
		const float3 dPdu = minDiff(P, Pr, Pl);
		const float3 dPdv = minDiff(P, Pt, Pb);
		
		//const uvec2 encodedTBN = texelFetch(usampler2D(uTangentSpaceImage,  uPointSampler), ivec2(gl_GlobalInvocationID.xy), 0).xy;
		const float3 N = normalize(cross(dPdu, dPdv));//decodeNormal((encodedTBN.xy * (1.0 / 1023.0)) * 2.0 - 1.0);//normalize(cross(dPdu, dPdv));
		
		// project normal onto slice plane
		// invert dir.y because screen space y axis points down, but view space y axis points up
		const float3 planeN = normalize(cross(float3(dir.x, -dir.y, 0.0), V));
		float3 projectedN = N - dot(N, planeN) * planeN;
		
		float projectedNLength = length(projectedN);
		float invLength = 1.0 / (projectedNLength + 1e-6);
		projectedN *= invLength;
		
		// calculate gamma
		float3 tangent = cross(V, planeN);
		float cosGamma	= dot(projectedN, V);
		float gamma = acos(cosGamma) * sign(-dot(projectedN, tangent));
		float sinGamma2	= 2.0 * sin(gamma);
		
		
		// clamp horizons
		horizons.x = gamma + max(horizons.x - gamma, -PI * 0.5);
		horizons.y = gamma + min(horizons.y - gamma, PI * 0.5);
		
		float2 horizonCosTerm = (sinGamma2 * horizons - cos(2.0 * horizons - gamma)) + cosGamma;
		
		// premultiply
		projectedNLength *= 0.25;
		
		ao += projectedNLength * horizonCosTerm.x;
		ao += projectedNLength * horizonCosTerm.y;
	}

	g_ResultImage[threadID.xy] = float2(ao, -P.z);
}
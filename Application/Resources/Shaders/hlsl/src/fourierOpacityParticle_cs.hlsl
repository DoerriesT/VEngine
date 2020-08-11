#include "bindingHelper.hlsli"
#include "common.hlsli"
#include "commonEncoding.hlsli"
#include "commonFourierOpacity.hlsli"

struct ParticleData
{
	float3 position;
	float opacity;
	uint textureIndex;
	float pad0;
	float pad1;
	float pad2;
};

struct LightInfo
{
	float4x4 viewProjection;
	float4x4 invViewProjection;
	float3 position;
	float radius;
	float texelSize;
	uint resolution;
	uint offsetX;
	uint offsetY;
	uint isPointLight;
	float pad1;
	float pad2;
	float pad3;
};

struct PushConsts
{
	uint lightIndex;
	uint particleCount;
};

RWTexture2DArray<float4> g_ResultImage : REGISTER_UAV(0, 0);
StructuredBuffer<LightInfo> g_LightInfo : REGISTER_SRV(1, 0);
StructuredBuffer<ParticleData> g_Particles : REGISTER_SRV(2, 0);

Texture2D<float4> g_Textures[TEXTURE_ARRAY_SIZE] : REGISTER_SRV(0, 1);
SamplerState g_Samplers[SAMPLER_COUNT] : REGISTER_SAMPLER(1, 1);


PUSH_CONSTS(PushConsts, g_PushConsts);

groupshared uint s_culledParticleIndices[64];
groupshared uint s_culledParticleCount;


void writeResult(uint2 dstCoord, uint2 localOffset, uint resolution, bool octahedronMap, float4 result0, float4 result1)
{
	g_ResultImage[uint3(dstCoord, 0)] += result0;
	g_ResultImage[uint3(dstCoord, 1)] += result1;
	
	// replicate border texels in outer "gutter" region to ensure correct linear sampling
	if (octahedronMap)
	{
		uint2 localCoord = dstCoord - localOffset;
		const bool4 border = bool4(localCoord.x == 0, localCoord.y == 0, localCoord.x == (resolution - 1), localCoord.y == (resolution - 1));
		
		const int2 offset = int2(localCoord) * -2 + (int(resolution) - 1);
		
		// horizontal border
		if (border.x || border.z)
		{
			const int2 horizontalBorderCoord = border.x ? (dstCoord + int2(-1, offset.y)) : (dstCoord + int2(1, offset.y));
			g_ResultImage[uint3(horizontalBorderCoord, 0)] += result0;
			g_ResultImage[uint3(horizontalBorderCoord, 1)] += result1;
		}
		
		// vertical border
		if (border.y || border.w)
		{
			const int2 verticalBorderCoord = border.y ? (dstCoord + int2(offset.x, -1)) : dstCoord + int2(offset.x, 1);
			g_ResultImage[uint3(verticalBorderCoord, 0)] += result0;
			g_ResultImage[uint3(verticalBorderCoord, 1)] += result1;
		}
		
		// corner
		if ((border.x && border.y) || (border.x && border.w) || (border.z && border.y) || (border.z && border.w))
		{
			const int2 cornerCoord = dstCoord + int2(border.xy ? int2(resolution, resolution) : int2(-resolution, -resolution));
			g_ResultImage[uint3(cornerCoord, 0)] += result0;
			g_ResultImage[uint3(cornerCoord, 1)] += result1;
		}
	}
}

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID, uint3 groupID : SV_GroupID, uint groupThreadIndex : SV_GroupIndex, uint3 groupThreadID : SV_GroupThreadID)
{
	const LightInfo lightInfo = g_LightInfo[g_PushConsts.lightIndex];
	
	const uint2 dstCoord = threadID.xy + uint2(lightInfo.offsetX, lightInfo.offsetY);
	
	const float2 texCoord = (threadID.xy + 0.5) * lightInfo.texelSize;
	
	// compute ray from light source passing through the current pixel
	float3 ray = 0.0;
	if (lightInfo.isPointLight)
	{
		ray = -decodeOctahedron(texCoord * 2.0 - 1.0);
	}
	else
	{
		const float4 farPlaneWorldSpacePos = mul(lightInfo.invViewProjection, float4(texCoord * float2(2.0, -2.0) - float2(1.0, -1.0), 1.0, 1.0));
		ray = normalize((farPlaneWorldSpacePos.xyz / farPlaneWorldSpacePos.w) - lightInfo.position);
	}
	
	const float invRadius = 1.0 / lightInfo.radius;
	
	float4 result0 = 0.0;
	float4 result1 = 0.0;
	
	// rasterize particles
	if (threadID.x < lightInfo.resolution && threadID.y < lightInfo.resolution)
	{
		for (uint i = 0; i < g_PushConsts.particleCount; ++i)
		{
			ParticleData particle = g_Particles[i];
			
			float3 particleNormal = particle.position - lightInfo.position;
			float distToParticle = length(particleNormal);
			particleNormal *= rcp(distToParticle);
			
			float t = dot((particle.position - lightInfo.position), particleNormal) / dot(ray, particleNormal);
			float3 intersectionPos = lightInfo.position + t * ray;
			
			float distToParticleCenter = distance(particle.position, intersectionPos);
			
			float particleSize = 0.4;
			
			float3 bitangent;
			if (abs(particleNormal.x) <= abs(particleNormal.y) && abs(particleNormal.x) <= abs(particleNormal.z))
			{
				bitangent = float3(0.0, -particleNormal.z, particleNormal.y);
			}
			else if (abs(particleNormal.y) <= abs(particleNormal.z) && abs(particleNormal.y) <= abs(particleNormal.x))
			{
				bitangent = float3(-particleNormal.z, 0.0, particleNormal.x);
			}
			else
			{
				bitangent = float3(-particleNormal.y, particleNormal.x, 0.0);
			}
			bitangent = normalize(bitangent);
			
			float3 tangent = cross(bitangent, particleNormal);
			
			float3x3 rotation = float3x3(tangent, bitangent, particleNormal);
			
			float2 localPos = mul(rotation, intersectionPos - particle.position).xy / particleSize;
			
			if (t > 0.0 && all(abs(localPos) < 1.0))
			{
				float opacity = particle.opacity * 0.2;
				
				if (particle.textureIndex != 0)
				{
					opacity *= g_Textures[particle.textureIndex - 1].SampleLevel(g_Samplers[SAMPLER_LINEAR_REPEAT], localPos * 0.5 + 0.5, 0.0).a;
				}
				
				float transmittance = 1.0 - opacity;
				float depth = distToParticle * invRadius;
				fourierOpacityAccumulate(depth, transmittance, result0, result1);
			}
		}
	}
	
	//int remainingParticleCount = g_PushConsts.particleCount;
	//uint currentOffset;
	//while (remainingParticleCount > 0)
	//{
	//	// clear shared memory
	//	{
	//		if (groupThreadIndex == 0)
	//		{
	//			s_culledParticleCount = 0;
	//		}
	//	}
	//	
	//	GroupMemoryBarrierWithGroupSync();
	//	
	//	// cull particles
	//	{
	//		uint particleIndex = currentOffset + groupThreadIndex;
	//		bool intersectsTile = false;
	//		
	//		if (particleIndex < g_PushConsts.particleCount)
	//		{
	//			// test against tile
	//		}
	//		
	//		if (intersectsTile)
	//		{
	//			uint index = 0;
	//			InterlockedAdd(s_culledParticleCount, 1, index);
	//			s_culledParticleIndices[index] = particleIndex;
	//		}
	//	}
	//	
	//	GroupMemoryBarrierWithGroupSync();
	//	
	//	// rasterize particles
	//	if (threadID.x < lightInfo.resolution && threadID.y < lightInfo.resolution)
	//	{
	//		for (uint i = 0; i < s_culledParticleCount; ++i)
	//		{
	//			ParticleData particle = g_Particles[i];
	//		
	//			float transmittance = 1.0 - particle.opacity;
	//			float depth = distance(particle.position, lightInfo.position) * invRadius;
	//			fourierOpacityAccumulate(depth, transmittance, result0, result1);
	//		}
	//	}
	//	
	//	remainingParticleCount -= 64;
	//	if (remainingParticleCount > 0)
	//	{
	//		GroupMemoryBarrierWithGroupSync();
	//	}
	//}
	
	if (threadID.x < lightInfo.resolution && threadID.y < lightInfo.resolution)
	{
		writeResult(dstCoord, uint2(lightInfo.offsetX, lightInfo.offsetY), lightInfo.resolution, (bool)lightInfo.isPointLight, result0, result1);
	}
}
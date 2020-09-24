#include "bindingHelper.hlsli"
#include "fourierOpacityLocal.hlsli"
#include "common.hlsli"
#include "commonEncoding.hlsli"
#include "commonFourierOpacity.hlsli"
#include "commonVolumetricFog.hlsli"

RWTexture2DArray<float4> g_ResultImage : REGISTER_UAV(RESULT_IMAGE_BINDING, 0);
StructuredBuffer<LightInfo> g_LightInfo : REGISTER_SRV(LIGHT_INFO_BINDING, 0);
StructuredBuffer<GlobalParticipatingMedium> g_GlobalMedia : REGISTER_SRV(GLOBAL_MEDIA_BINDING, 0);
StructuredBuffer<LocalParticipatingMedium> g_LocalMedia : REGISTER_SRV(LOCAL_MEDIA_BINDING, 0);
StructuredBuffer<ParticleData> g_Particles : REGISTER_SRV(PARTICLE_DATA_BINDING, 0);

Texture3D<float4> g_Textures3D[TEXTURE_ARRAY_SIZE] : REGISTER_SRV(0, 1);
Texture2D<float4> g_Textures[TEXTURE_ARRAY_SIZE] : REGISTER_SRV(0, 2);
SamplerState g_Samplers[SAMPLER_COUNT] : REGISTER_SAMPLER(0, 3);

PUSH_CONSTS(PushConsts, g_PushConsts);

void writeResult(uint2 dstCoord, uint2 localOffset, uint resolution, bool octahedronMap, float4 result0, float4 result1)
{
	g_ResultImage[uint3(dstCoord, 0)] = result0;
	g_ResultImage[uint3(dstCoord, 1)] = result1;
	
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
			g_ResultImage[uint3(horizontalBorderCoord, 0)] = result0;
			g_ResultImage[uint3(horizontalBorderCoord, 1)] = result1;
		}
		
		// vertical border
		if (border.y || border.w)
		{
			const int2 verticalBorderCoord = border.y ? (dstCoord + int2(offset.x, -1)) : dstCoord + int2(offset.x, 1);
			g_ResultImage[uint3(verticalBorderCoord, 0)] = result0;
			g_ResultImage[uint3(verticalBorderCoord, 1)] = result1;
		}
		
		// corner
		if ((border.x && border.y) || (border.x && border.w) || (border.z && border.y) || (border.z && border.w))
		{
			const int2 cornerCoord = dstCoord + int2(border.xy ? int2(resolution, resolution) : int2(-resolution, -resolution));
			g_ResultImage[uint3(cornerCoord, 0)] = result0;
			g_ResultImage[uint3(cornerCoord, 1)] = result1;
		}
	}
}

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID, uint3 groupID : SV_GroupID, uint groupThreadIndex : SV_GroupIndex, uint3 groupThreadID : SV_GroupThreadID)
{
	uint2 globalCoord = threadID.xy;
	
	const LightInfo lightInfo = g_LightInfo[g_PushConsts.lightIndex];
	if (globalCoord.x >= lightInfo.resolution || globalCoord.y >= lightInfo.resolution)
	{
		return;
	}
	
	uint2 dstCoord = globalCoord.xy + uint2(lightInfo.offsetX, lightInfo.offsetY);
	const float2 texCoord = (globalCoord.xy + 0.5) * lightInfo.texelSize;
	const uint depthCoord = 0;
	
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
	
	// ray segment of current thread
	const float segmentLength = lightInfo.radius / 4.0;
	float rayStart = segmentLength * depthCoord;
	const float rayEnd = segmentLength * (depthCoord + 1);
	const float invRadius = 1.0 / lightInfo.radius;
	
	float4 result0 = 0.0;
	float4 result1 = 0.0;
	
	// march ray
	//const float stepSize = 0.1;
	//const int stepCount = (int)ceil((rayEnd - rayStart) / stepSize);
	const int stepCount = clamp(ceil(segmentLength / 0.05), 1, 64);
	const float stepSize = (rayEnd - rayStart) / stepCount;
	
	//float2x2 ditherMatrix = float2x2(0.25, 0.75, 1.0, 0.5);
	//float dither = ditherMatrix[groupID.y % 2][groupID.x % 2] * stepSize;
	
	float4x4 ditherMatrix = float4x4(1 / 16.0, 9 / 16.0, 3 / 16.0, 11 / 16.0,
									13 / 16.0, 5 / 16.0, 15 / 16.0, 7 / 16.0,
									4 / 16.0, 12 / 16.0, 2 / 16.0, 10 / 16.0,
									16 / 16.0, 8 / 16.0, 14 / 16.0, 6 / 16.0);
	float dither = ditherMatrix[globalCoord.y % 4][globalCoord.x % 4] * stepSize;
	
	rayStart += dither + stepSize * 0.5;
	
	// raymarch volumes
	{
		for (int i = 0; i < stepCount; ++i)
		{
			float t = i * stepSize + rayStart;
			float3 rayPos = lightInfo.position + ray * t;
			
			float extinction = 0.0;
			
			// add extinction of all global volumes
			{
				for (int j = 0; j < g_PushConsts.globalMediaCount; ++j)
				{
					GlobalParticipatingMedium medium = g_GlobalMedia[j];
					const float density = volumetricFogGetDensity(medium, rayPos, g_Textures3D, g_Samplers[SAMPLER_LINEAR_CLAMP]);
					extinction += medium.extinction * density;
				}
			}
			
			// add extinction of all local volumes
			for (int j = 0; j < g_PushConsts.localVolumeCount; ++j)
			{
				LocalParticipatingMedium medium = g_LocalMedia[j];
				
				const float3 localPos = float3(dot(medium.worldToLocal0, float4(rayPos, 1.0)), 
										dot(medium.worldToLocal1, float4(rayPos, 1.0)), 
										dot(medium.worldToLocal2, float4(rayPos, 1.0)));
										
				if (all(abs(localPos) <= 1.0) && (medium.spherical == 0 || dot(localPos, localPos) <= 1.0))
				{
					float density = volumetricFogGetDensity(medium, localPos, g_Textures3D, g_Samplers[SAMPLER_LINEAR_CLAMP]);
					extinction += medium.extinction * density;
				}
			}
			
			float transmittance = exp(-extinction * stepSize);
			float depth = t * invRadius;
			fourierOpacityAccumulate(depth, transmittance, result0, result1);
		}
	}
	
	
	// rasterize particles
	{
		for (int i = 0; i < g_PushConsts.particleCount; ++i)
		{
			ParticleData particle = g_Particles[i];
			
			// compute particle normal (faces light position)
			float3 particleNormal = particle.position - lightInfo.position;
			float distToParticle = length(particleNormal);
			particleNormal *= rcp(distToParticle);
			
			// intersect ray with plane formed by particle normal and distance
			float t = dot((particle.position - lightInfo.position), particleNormal) / dot(ray, particleNormal);
			float3 intersectionPos = lightInfo.position + t * ray;
			
			float distToParticleCenter = distance(particle.position, intersectionPos);
			
			// compute orthonormal tangent frame
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
			
			// transform intersection position to tangent space and normalize extent by particle size
			float3x3 rotation = float3x3(tangent, bitangent, particleNormal);
			float2 localPos = mul(rotation, intersectionPos - particle.position).xy / particle.size;
			
			// is the ray intersection inside the particle area?
			if (t > 0.0 && all(abs(localPos) < 1.0))
			{
				// apply particle rotation
				float cosRot;
				float sinRot;
				sincos(particle.rotation, sinRot, cosRot);
				float2x2 particleRot = float2x2(cosRot, -sinRot, sinRot, cosRot);
				localPos = mul(localPos, particleRot);
				
				// get particle opacity
				float opacity = particle.opacity * particle.fomOpacityMult;
				if (particle.textureIndex != 0)
				{
					opacity *= g_Textures[particle.textureIndex - 1].SampleLevel(g_Samplers[SAMPLER_LINEAR_REPEAT], localPos * 0.5 + 0.5, 0.0).a;
				}
				
				// accumulate
				float transmittance = 1.0 - opacity;
				float depth = distToParticle * invRadius;
				fourierOpacityAccumulate(depth, transmittance, result0, result1);
			}
		}
	}
	
	writeResult(dstCoord, uint2(lightInfo.offsetX, lightInfo.offsetY), lightInfo.resolution, (bool)lightInfo.isPointLight, result0, result1);
}
#include "bindingHelper.hlsli"
#include "fourierOpacityParticleDirectional.hlsli"
#include "common.hlsli"

struct VSOutput
{
	float4 position : SV_Position;
	float2 texCoord : TEXCOORD;
	nointerpolation float opacity : OPACITY;
	nointerpolation uint textureIndex : TEXTURE_INDEX;
	nointerpolation uint depthRangeLayer : DEPTH_RANGE_LAYER;
};

StructuredBuffer<ParticleData> g_Particles : REGISTER_SRV(PARTICLES_BINDING, 0);
StructuredBuffer<float4x4> g_ShadowMatrices : REGISTER_SRV(MATRIX_BUFFER_BINDING, 0);
StructuredBuffer<float4> g_LightDirections : REGISTER_SRV(LIGHT_DIR_BUFFER_BINDING, 0);

PUSH_CONSTS(PushConsts, g_PushConsts);

VSOutput main(uint vertexID : SV_VertexID) 
{
	uint particleID = vertexID / 6;
	uint positionIndex = vertexID % 6;
	
	float2 positions[6] = 
	{ 
		float2(-1.0, 1.0), 
		float2(-1.0, -1.0), 
		float2(1.0, 1.0),
		float2(1.0, 1.0),
		float2(-1.0, -1.0), 
		float2(1.0, -1.0),
	};
	
	ParticleData particle = g_Particles[particleID + g_PushConsts.particleOffset];
	
	float cosRot;
	float sinRot;
	sincos(particle.rotation, sinRot, cosRot);
	float2x2 particleRot = float2x2(cosRot, -sinRot, sinRot, cosRot);
	
	float3 pos = float3(mul(positions[positionIndex], particleRot), 0.0) * particle.size;
	
	float3 normal = g_LightDirections[g_PushConsts.directionIndex * 2].xyz;
	float3 up = g_LightDirections[g_PushConsts.directionIndex * 2 + 1].xyz;
	float3 tangent = cross(up, normal);
	
	float3x3 rotation = float3x3(tangent, up, normal);
	pos = mul(pos, rotation);
	
	VSOutput output = (VSOutput)0;
	output.position = mul(g_ShadowMatrices[g_PushConsts.shadowMatrixIndex], float4(pos + particle.position, 1.0));
	output.texCoord = positions[positionIndex] * float2(0.5, -0.5) + 0.5;
	output.opacity = particle.opacity * particle.fomOpacityMult;
	output.textureIndex = particle.textureIndex;
	output.depthRangeLayer = g_PushConsts.shadowMatrixIndex;
	
	return output;
}
#include "bindingHelper.hlsli"
#include "particles.hlsli"

struct VSOutput
{
	float4 position : SV_Position;
	float2 texCoord : TEXCOORD;
	float opacity : OPACITY;
	float3 worldSpacePos : WORLD_SPACE_POS;
	float3 viewSpacePos : VIEW_SPACE_POS;
	nointerpolation uint textureIndex : TEXTURE_INDEX;
};

struct PushConsts
{
	uint particleOffset;
};

struct ParticleData
{
	float3 position;
	float opacity;
	uint textureIndex;
	float pad0;
	float pad1;
	float pad2;
};

ConstantBuffer<Constants> g_Constants : REGISTER_CBV(CONSTANT_BUFFER_BINDING, 0);
StructuredBuffer<ParticleData> g_Particles : REGISTER_SRV(PARTICLES_BINDING, 0);

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
	float3 pos = float3(positions[positionIndex], 0.0) * 0.4;
	
	float3 normal = normalize(g_Constants.cameraPosition - particle.position);
	float3 tangent = cross(g_Constants.cameraUp, normal);
	
	float3x3 rotation = float3x3(tangent, g_Constants.cameraUp, normal);
	pos = mul(pos, rotation);
	
	VSOutput output = (VSOutput)0;
	output.position = mul(g_Constants.viewProjectionMatrix, float4(pos + particle.position, 1.0));
	output.texCoord = positions[positionIndex] * float2(0.5, -0.5) + 0.5;
	output.opacity = particle.opacity;
	output.textureIndex = particle.textureIndex;
	output.worldSpacePos = pos + particle.position;
	output.viewSpacePos = mul(g_Constants.viewMatrix, float4(pos + particle.position, 1.0)).xyz;
	
	return output;
}
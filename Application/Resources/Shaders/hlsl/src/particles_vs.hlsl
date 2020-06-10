#include "bindingHelper.hlsli"
#include "particles.hlsli"

struct VSOutput
{
	float4 position : SV_Position;
	float2 texCoord : TEXCOORD;
	float opacity : OPACITY;
};

struct PushConsts
{
	uint particleOffset;
};

ConstantBuffer<Constants> g_Constants : REGISTER_CBV(CONSTANT_BUFFER_BINDING, 0);
StructuredBuffer<float4> g_Particles : REGISTER_SRV(PARTICLES_BINDING, 0);

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
	
	float4 particle = g_Particles[particleID + g_PushConsts.particleOffset];
	float3 pos = float3(positions[positionIndex], 0.0) * 0.1;
	
	float3 normal = normalize(g_Constants.cameraPosition - (pos + particle.xyz));
	float3 tangent = cross(g_Constants.cameraUp, normal);
	
	float3x3 rotation = float3x3(tangent, g_Constants.cameraUp, normal);
	pos = mul(pos, rotation);
	
	VSOutput output = (VSOutput)0;
	output.position = mul(g_Constants.viewProjectionMatrix, float4(pos + particle.xyz, 1.0));
	output.texCoord = positions[positionIndex] * float2(0.5, -0.5) + 0.5;
	output.opacity = particle.w;
	
	return output;
}
#include "bindingHelper.hlsli"
#include "forward.hlsli"

struct VSOutput
{
	float4 position : SV_Position;
	float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
	float4 worldPos : WORLD_POSITION;
	nointerpolation uint materialIndex : MATERIAL_INDEX;
};

ConstantBuffer<Constants> g_Constants : REGISTER_CBV(CONSTANT_BUFFER_BINDING, CONSTANT_BUFFER_SET);
StructuredBuffer<float4x4> g_TransformMatrices : REGISTER_SRV(TRANSFORM_DATA_BINDING, TRANSFORM_DATA_SET);
StructuredBuffer<float> g_Positions : REGISTER_SRV(VERTEX_POSITIONS_BINDING, VERTEX_POSITIONS_SET);
StructuredBuffer<float> g_Normals : REGISTER_SRV(VERTEX_NORMALS_BINDING, VERTEX_NORMALS_SET);
StructuredBuffer<float> g_TexCoords : REGISTER_SRV(VERTEX_TEXCOORDS_BINDING, VERTEX_TEXCOORDS_SET);

PUSH_CONSTS(PushConsts, g_PushConsts);

VSOutput main(uint vertexID : SV_VertexID) 
{
	VSOutput output;
	
	const float4x4 modelMatrix = g_TransformMatrices[g_PushConsts.transformIndex];
	const float3 position = float3(g_Positions[vertexID * 3 + 0], g_Positions[vertexID * 3 + 1], g_Positions[vertexID * 3 + 2]);
    
	const float4x4 viewMatrix = transpose(float4x4(g_Constants.viewMatrixRow0, g_Constants.viewMatrixRow1, g_Constants.viewMatrixRow2, float4(0.0, 0.0, 0.0, 1.0)));
	const float4x4 modelViewMatrix = mul(viewMatrix, modelMatrix);										
	
	output.position = mul(g_Constants.jitteredViewProjectionMatrix, mul(modelMatrix, float4(position, 1.0)));
	output.texCoord = float2(g_TexCoords[vertexID * 2 + 0], g_TexCoords[vertexID * 2 + 1]);
	output.normal = mul(float3(g_Normals[vertexID * 3 + 0], g_Normals[vertexID * 3 + 1], g_Normals[vertexID * 3 + 2]), (float3x3)modelViewMatrix);
	output.worldPos = mul(float4(position, 1.0), modelViewMatrix);
	output.materialIndex = g_PushConsts.materialIndex;
	
	return output;
}
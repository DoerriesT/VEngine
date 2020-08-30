#define TRANSFORM_DATA_BINDING 0
#define MATERIAL_DATA_BINDING 1
#define VERTEX_POSITIONS_BINDING 2
#define VERTEX_TEXCOORDS_BINDING 3

struct PushConsts
{
	float4x4 shadowMatrix;
	uint transformIndex;
	uint materialIndex;
	uint vertexOffset;
	uint pad;
	float2 texCoordScale;
	float2 texCoordBias;
};
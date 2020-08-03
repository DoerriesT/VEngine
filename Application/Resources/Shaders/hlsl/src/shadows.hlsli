#define TRANSFORM_DATA_SET 0
#define TRANSFORM_DATA_BINDING 0
#define MATERIAL_DATA_SET 0
#define MATERIAL_DATA_BINDING 1
#define VERTEX_POSITIONS_SET 0
#define VERTEX_POSITIONS_BINDING 2
#define VERTEX_TEXCOORDS_SET 0
#define VERTEX_TEXCOORDS_BINDING 3

#define TEXTURES_SET 1
#define TEXTURES_BINDING 0
#define SAMPLERS_SET 1
#define SAMPLERS_BINDING 1

struct PushConsts
{
	float4x4 shadowMatrix;
	uint transformIndex;
	uint materialIndex;
	float2 texCoordScale;
	float2 texCoordBias;
};
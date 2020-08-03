#define CONSTANT_BUFFER_BINDING 0
#define TRANSFORM_DATA_BINDING 1
#define MATERIAL_DATA_BINDING 2
#define VERTEX_POSITIONS_BINDING 3
#define VERTEX_TEXCOORDS_BINDING 4

#define TEXTURES_SET 1
#define TEXTURES_BINDING 0
#define SAMPLERS_SET 1
#define SAMPLERS_BINDING 1

struct Constants
{
	float4x4 jitteredViewProjectionMatrix;
};

struct PushConsts
{
	float2 texCoordScale;
	float2 texCoordBias;
	uint transformIndex;
	uint materialIndex;
	uint instanceID;
};
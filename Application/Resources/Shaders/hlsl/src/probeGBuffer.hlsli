#define TRANSFORM_DATA_SET 0
#define TRANSFORM_DATA_BINDING 0
#define MATERIAL_DATA_SET 0
#define MATERIAL_DATA_BINDING 1
#define VERTEX_POSITIONS_SET 0
#define VERTEX_POSITIONS_BINDING 2
#define VERTEX_NORMALS_SET 0
#define VERTEX_NORMALS_BINDING 3
#define VERTEX_TEXCOORDS_SET 0
#define VERTEX_TEXCOORDS_BINDING 4
#define CONSTANT_BUFFER_SET 0
#define CONSTANT_BUFFER_BINDING 5

#define TEXTURES_SET 1
#define TEXTURES_BINDING 0
#define SAMPLERS_SET 1
#define SAMPLERS_BINDING 1

struct PushConsts
{
	uint face;
	uint transformIndex;
	uint materialIndex;
};

struct Constants
{
	float4x4 viewProjectionMatrix[6];
};
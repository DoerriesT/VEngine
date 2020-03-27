#define INSTANCE_DATA_SET 0
#define INSTANCE_DATA_BINDING 0
#define TRANSFORM_DATA_SET 0
#define TRANSFORM_DATA_BINDING 1
#define MATERIAL_DATA_SET 0
#define MATERIAL_DATA_BINDING 2
#define VERTEX_POSITIONS_SET 0
#define VERTEX_POSITIONS_BINDING 3
#define VERTEX_NORMALS_SET 0
#define VERTEX_NORMALS_BINDING 4
#define VERTEX_TEXCOORDS_SET 0
#define VERTEX_TEXCOORDS_BINDING 5
#define SUB_MESH_DATA_SET 0
#define SUB_MESH_DATA_BINDING 6
#define DEFERRED_SHADOW_IMAGE_SET 0
#define DEFERRED_SHADOW_IMAGE_BINDING 7
#define DIRECTIONAL_LIGHT_DATA_SET 0
#define DIRECTIONAL_LIGHT_DATA_BINDING 8
#define POINT_LIGHT_DATA_SET 0
#define POINT_LIGHT_DATA_BINDING 9
#define POINT_LIGHT_Z_BINS_BUFFER_SET 0
#define POINT_LIGHT_Z_BINS_BUFFER_BINDING 10
#define POINT_LIGHT_BIT_MASK_BUFFER_SET 0
#define POINT_LIGHT_BIT_MASK_BUFFER_BINDING 11
#define VOLUMETRIC_FOG_IMAGE_SET 0
#define VOLUMETRIC_FOG_IMAGE_BINDING 13
#define SSAO_IMAGE_SET 0
#define SSAO_IMAGE_BINDING 14
#define SPOT_LIGHT_DATA_SET 0
#define SPOT_LIGHT_DATA_BINDING 15
#define SPOT_LIGHT_Z_BINS_BUFFER_SET 0
#define SPOT_LIGHT_Z_BINS_BUFFER_BINDING 16
#define SPOT_LIGHT_BIT_MASK_BUFFER_SET 0
#define SPOT_LIGHT_BIT_MASK_BUFFER_BINDING 17
#define CONSTANT_BUFFER_SET 0
#define CONSTANT_BUFFER_BINDING 18
#define EXPOSURE_DATA_BUFFER_SET 0
#define EXPOSURE_DATA_BUFFER_BINDING 19

#define TEXTURES_SET 1
#define TEXTURES_BINDING 0
#define SAMPLERS_SET 1
#define SAMPLERS_BINDING 1

struct PushConsts
{
	uint transformIndex;
	uint materialIndex;
};

struct Constants
{
	float4x4 jitteredViewProjectionMatrix;
	float4 viewMatrixRow0;
	float4 viewMatrixRow1;
	float4 viewMatrixRow2;
	uint directionalLightCount;
	uint pointLightCount;
	uint ambientOcclusion;
	uint width;
};
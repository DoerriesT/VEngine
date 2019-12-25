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
#define VOXEL_PTR_IMAGE_SET 0
#define VOXEL_PTR_IMAGE_BINDING 7
#define BIN_VIS_IMAGE_BUFFER_SET 0
#define BIN_VIS_IMAGE_BUFFER_BINDING 8
#define SHADOW_IMAGE_SET 0
#define SHADOW_IMAGE_BINDING 9
#define SHADOW_SAMPLER_SET 0
#define SHADOW_SAMPLER_BINDING 10
#define DIRECTIONAL_LIGHT_DATA_SET 0
#define DIRECTIONAL_LIGHT_DATA_BINDING 11
#define SHADOW_MATRICES_SET 0
#define SHADOW_MATRICES_BINDING 12
#define COLOR_IMAGE_BUFFER_SET 0
#define COLOR_IMAGE_BUFFER_BINDING 13

#define TEXTURES_SET 1
#define TEXTURES_BINDING 0
#define SAMPLERS_SET 1
#define SAMPLERS_BINDING 1

#define DIRECTIONAL_LIGHT_COUNT_CONST_ID 0
#define VOXEL_GRID_WIDTH_CONST_ID 1
#define VOXEL_GRID_HEIGHT_CONST_ID 2
#define VOXEL_GRID_DEPTH_CONST_ID 3
#define VOXEL_SCALE_CONST_ID 4

struct PushConsts
{
	mat4 invViewMatrix;
	vec3 gridOffset;
	float invGridResolution;
	uint superSamplingFactor;
	uint transformIndex;
	uint materialIndex;
};
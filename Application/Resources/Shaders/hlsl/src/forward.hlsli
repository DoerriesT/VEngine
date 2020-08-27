#define INSTANCE_DATA_BINDING 0
#define TRANSFORM_DATA_BINDING 1
#define MATERIAL_DATA_BINDING 2
#define VERTEX_POSITIONS_BINDING 3
#define VERTEX_NORMALS_BINDING 4
#define VERTEX_TEXCOORDS_BINDING 5
#define SUB_MESH_DATA_BINDING 6
#define DEFERRED_SHADOW_IMAGE_BINDING 7
#define DIRECTIONAL_LIGHTS_BINDING 8
#define DIRECTIONAL_LIGHTS_SHADOWED_BINDING 9
#define PUNCTUAL_LIGHTS_BINDING 10
#define PUNCTUAL_LIGHTS_Z_BINS_BINDING 11
#define PUNCTUAL_LIGHTS_BIT_MASK_BINDING 12
#define PUNCTUAL_LIGHTS_SHADOWED_BINDING 13
#define PUNCTUAL_LIGHTS_SHADOWED_Z_BINS_BINDING 14
#define PUNCTUAL_LIGHTS_SHADOWED_BIT_MASK_BINDING 15
#define VOLUMETRIC_FOG_IMAGE_BINDING 16
#define SSAO_IMAGE_BINDING 17
#define CONSTANT_BUFFER_BINDING 18
#define EXPOSURE_DATA_BUFFER_BINDING 19
#define SHADOW_ATLAS_IMAGE_BINDING 20
#define FOM_IMAGE_BINDING 21
#define VERTEX_QTANGENTS_BINDING 22

struct PushConsts
{
	float2 texCoordScale;
	float2 texCoordBias;
	uint transformIndex;
	uint materialIndex;
	uint vertexOffset;
};

struct Constants
{
	float4x4 jitteredViewProjectionMatrix;
	float4x4 invViewMatrix;
	float4x4 viewMatrix;
	uint directionalLightCount;
	uint directionalLightShadowedCount;
	uint punctualLightCount;
	uint punctualLightShadowedCount;
	uint ambientOcclusion;
	uint width;
	float coordScale;
	int volumetricShadow;
	float3 coordBias;
	float extinctionVolumeTexelSize;
};
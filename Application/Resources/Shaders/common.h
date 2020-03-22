#ifndef TEXTURE_ARRAY_SIZE
#define TEXTURE_ARRAY_SIZE (1024)
#endif // TEXTURE_ARRAY_SIZE

#ifndef SAMPLER_COUNT
#define SAMPLER_COUNT (4)
#endif // SAMPLER_COUNT

#ifndef IRRADIANCE_VOLUME_RAY_MARCHING_RAY_COUNT
#define IRRADIANCE_VOLUME_RAY_MARCHING_RAY_COUNT (256)
#endif // IRRADIANCE_VOLUME_RAY_MARCHING_RAY_COUNT

#define SAMPLER_LINEAR_CLAMP (0)
#define SAMPLER_LINEAR_REPEAT (1)
#define SAMPLER_POINT_CLAMP (2)
#define SAMPLER_POINT_REPEAT (3)

#ifndef PI
#define PI (3.14159265359)
#endif // PI

#ifndef TILE_SIZE
#define TILE_SIZE 16
#endif // TILE_SIZE

#ifndef LUMINANCE_HISTOGRAM_SIZE
#define LUMINANCE_HISTOGRAM_SIZE 256
#endif // LUMINANCE_HISTOGRAM_SIZE

#ifndef ALPHA_CUTOFF
#define ALPHA_CUTOFF (0.9)
#endif // ALPHA_CUTOFF

#ifndef ALPHA_MIP_SCALE
#define ALPHA_MIP_SCALE (0.25)
#endif // MIP_SCALE

struct DirectionalLightData
{
	vec4 color;
	vec4 direction;
	uint shadowDataOffsetCount;
};

struct PointLightData
{
	vec4 positionRadius;
	vec4 colorInvSqrAttRadius;
	//uint shadowDataOffset;
	//uint shadowDataCount;
};

struct SpotLightData
{
	vec4 colorInvSqrAttRadius;
	vec4 positionAngleScale;
	vec4 directionAngleOffset;
	//uint shadowDataOffset;
	//uint shadowDataCount;
};

struct MaterialData
{
	uint albedoOpacity;
	uint emissive;
	uint metalnessRoughness;
	uint albedoNormalTexture;
	uint metalnessRoughnessTexture;
	uint occlusionEmissiveTexture;
	uint displacementTexture;
	uint padding;
};

struct SubMeshInstanceData
{
	uint subMeshIndex;
	uint transformIndex;
	uint materialIndex;
};
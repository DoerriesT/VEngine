#ifndef TEXTURE_ARRAY_SIZE
#define TEXTURE_ARRAY_SIZE (1024)
#endif // TEXTURE_ARRAY_SIZE

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

struct ShadowData
{
	mat4 shadowViewProjectionMatrix;
	vec4 shadowCoordScaleBias;
};

struct DirectionalLightData
{
	vec4 color;
	vec4 direction;
	uint shadowDataOffset;
	uint shadowDataCount;
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
	vec4 boundingSphere;
	//uint shadowDataOffset;
	//uint shadowDataCount;
};

struct MaterialData
{
	vec4 albedoMetalness;
	vec4 emissiveRoughness;
	uint albedoNormalTexture;
	uint metalnessRoughnessTexture;
	uint occlusionEmissiveTexture;
	uint displacementTexture;
};

struct SubMeshInstanceData
{
	uint subMeshIndex;
	uint transformIndex;
	uint materialIndex;
};
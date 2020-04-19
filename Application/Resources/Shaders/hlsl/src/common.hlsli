#ifndef COMMON_H
#define COMMON_H

#ifndef PI
#define PI (3.14159265359)
#endif // PI

#ifndef TEXTURE_ARRAY_SIZE
#define TEXTURE_ARRAY_SIZE (1024)
#endif // TEXTURE_ARRAY_SIZE

#ifndef SAMPLER_COUNT
#define SAMPLER_COUNT (4)
#endif // SAMPLER_COUNT

#define SAMPLER_LINEAR_CLAMP (0)
#define SAMPLER_LINEAR_REPEAT (1)
#define SAMPLER_POINT_CLAMP (2)
#define SAMPLER_POINT_REPEAT (3)

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

struct DirectionalLight
{
	float3 color;
	uint shadowOffset;
	float3 direction;
	uint shadowCount;
};

struct PunctualLight
{
	float3 color;
	float invSqrAttRadius;
	float3 position;
	float angleScale;
	float3 direction;
	float angleOffset;
};

struct PunctualLightShadowed
{
	PunctualLight light;
	float4 shadowMatrix0;
	float4 shadowMatrix1;
	float4 shadowMatrix2;
	float4 shadowMatrix3;
	float4 shadowAtlasParams[6]; // x: scale, y: biasX, z: biasY, w: unused
	float3 positionWS;
	float radius;
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

struct GlobalParticipatingMedium
{
	float3 emissive;
	float extinction;
	float3 scattering;
	float phase;
};

struct LocalParticipatingMedium
{
	float3 obbAxis0;
	float extentX;
	float3 obbAxis1;
	float extentY;
	float3 obbAxis2;
	float extentZ;
	float3 position;
	float phase;
	float3 emissive;
	float extinction;
	float3 scattering;
	float pad0;
};

struct SubMeshInstanceData
{
	uint subMeshIndex;
	uint transformIndex;
	uint materialIndex;
};

#endif // COMMON_H
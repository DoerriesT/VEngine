#ifndef TEXTURE_ARRAY_SIZE
#define TEXTURE_ARRAY_SIZE (512)
#endif // TEXTURE_ARRAY_SIZE

#ifndef PI
#define PI (3.14159265359)
#endif // PI

#ifndef TILE_SIZE
#define TILE_SIZE 16
#endif // TILE_SIZE

struct ShadowData
{
	mat4 shadowViewProjectionMatrix;
	vec4 shadowCoordScaleBias;
};

struct DirectionalLight
{
	vec4 color;
	vec4 direction;
	uint shadowDataOffset;
	uint shadowDataCount;
};

struct PointLight
{
	vec4 positionRadius;
	vec4 colorInvSqrAttRadius;
	//uint shadowDataOffset;
	//uint shadowDataCount;
};

struct SpotLight
{
	vec4 colorInvSqrAttRadius;
	vec4 positionAngleScale;
	vec4 directionAngleOffset;
	vec4 boundingSphere;
	//uint shadowDataOffset;
	//uint shadowDataCount;
};

struct PerDrawData
{
	vec4 albedoFactorMetallic;
	vec4 emissiveFactorRoughness;
	mat4 modelMatrix;
	uint albedoTexture;
	uint normalTexture;
	uint metallicTexture;
	uint roughnessTexture;
	uint occlusionTexture;
	uint emissiveTexture;
	uint displacementTexture;
	uint padding;
};


layout(set = 0, binding = 0) uniform PER_FRAME_DATA 
{
	float time;
	float fovy;
	float nearPlane;
	float farPlane;
	mat4 viewMatrix;
	mat4 projectionMatrix;
	mat4 viewProjectionMatrix;
	mat4 invViewMatrix;
	mat4 invProjectionMatrix;
	mat4 invViewProjectionMatrix;
	mat4 prevViewMatrix;
	mat4 prevProjectionMatrix;
	mat4 prevViewProjectionMatrix;
	mat4 prevInvViewMatrix;
	mat4 prevInvProjectionMatrix;
	mat4 prevInvViewProjectionMatrix;
	vec4 cameraPosition;
	vec4 cameraDirection;
	uint width;
	uint height;
	uint frame;
	uint directionalLightCount;
	uint pointLightCount;
} uPerFrameData;

layout(set = 0, binding = 1, rgba16f) uniform writeonly image2D uLightingResultImage;

layout(set = 0, binding = 2) uniform sampler2D uGBufferTextures[4];

layout(set = 0, binding = 3) readonly buffer DIRECTIONAL_LIGHT_DATA
{
	DirectionalLight lights[];
} uDirectionalLightData;

layout(set = 0, binding = 4) readonly buffer POINT_LIGHT_DATA
{
	PointLight lights[];
} uPointLightData;

layout(set = 0, binding = 5) readonly buffer SHADOW_DATA
{
	ShadowData data[];
} uShadowData;

layout(set = 0, binding = 6) readonly buffer POINT_LIGHT_Z_BINS 
{
	uint bins[];
} uPointLightZBins;

layout(set = 0, binding = 7) buffer POINT_LIGHT_BITMASK 
{
	uint mask[];
} uPointLightBitMask;

layout(set = 0, binding = 8) uniform sampler2DShadow uShadowTexture;

layout(set = 0, binding = 9) uniform sampler2D uTextures[TEXTURE_ARRAY_SIZE];

layout(set = 0, binding = 10) readonly buffer PER_DRAW_DATA 
{
    PerDrawData data[];
} uPerDrawData;
#ifndef TEXTURE_ARRAY_SIZE
#define TEXTURE_ARRAY_SIZE (512)
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


#define LIGHTING_RESULT_TEXTURE_INDEX 4

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

struct MaterialData
{
	vec4 albedoMetalness;
	vec4 emissiveRoughness;
	uint albedoNormalTexture;
	uint metalnessRoughnessTexture;
	uint occlusionEmissiveTexture;
	uint displacementTexture;
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
	mat4 jitteredProjectionMatrix;
	mat4 jitteredViewProjectionMatrix;
	mat4 invJitteredProjectionMatrix;
	mat4 invJitteredViewProjectionMatrix;
	mat4 prevViewMatrix;
	mat4 prevProjectionMatrix;
	mat4 prevViewProjectionMatrix;
	mat4 prevInvViewMatrix;
	mat4 prevInvProjectionMatrix;
	mat4 prevInvViewProjectionMatrix;
	mat4 prevJitteredProjectionMatrix;
	mat4 prevJitteredViewProjectionMatrix;
	mat4 prevInvJitteredProjectionMatrix;
	mat4 prevInvJitteredViewProjectionMatrix;
	vec4 cameraPosition;
	vec4 cameraDirection;
	uint width;
	uint height;
	uint frame;
	uint directionalLightCount;
	uint pointLightCount;
	uint currentResourceIndex;
	uint previousResourceIndex;
	float timeDelta;
} uPerFrameData;

layout(set = 0, binding = 1) readonly buffer MATERIAL_DATA 
{
    MaterialData data[];
} uMaterialData;

layout(set = 0, binding = 2) readonly buffer TRANSFORM_DATA 
{
    mat4 data[];
} uTransformData;

layout(set = 0, binding = 3) readonly buffer SHADOW_DATA
{
	ShadowData data[];
} uShadowData;

layout(set = 0, binding = 4) readonly buffer DIRECTIONAL_LIGHT_DATA
{
	DirectionalLight lights[];
} uDirectionalLightData;

layout(set = 0, binding = 5) readonly buffer POINT_LIGHT_DATA
{
	PointLight lights[];
} uPointLightData;

layout(set = 0, binding = 6) readonly buffer POINT_LIGHT_Z_BINS 
{
	uint bins[];
} uPointLightZBins;

layout(set = 0, binding = 7) buffer POINT_LIGHT_BITMASK 
{
	uint mask[];
} uPointLightBitMask;

layout(set = 0, binding = 8) buffer PERSISTENT_VALUES
{
    float luminance[];
} uPersistentValues;

layout(set = 0, binding = 9) buffer LUMINANCE_HISTOGRAM
{
    uint data[];
} uLuminanceHistogram;

layout(set = 0, binding = 10) uniform sampler2D uTextures[TEXTURE_ARRAY_SIZE];

vec3 accurateLinearToSRGB(in vec3 linearCol)
{
	vec3 sRGBLo = linearCol * 12.92;
	vec3 sRGBHi = (pow(abs(linearCol), vec3(1.0/2.4)) * 1.055) - 0.055;
	vec3 sRGB = mix(sRGBLo, sRGBHi, vec3(greaterThan(linearCol, vec3(0.0031308))));
	return sRGB;
}

vec3 accurateSRGBToLinear(in vec3 sRGBCol)
{
	vec3 linearRGBLo = sRGBCol * (1.0 / 12.92);
	vec3 linearRGBHi = pow((sRGBCol + vec3(0.055)) * vec3(1.0 / 1.055), vec3(2.4));
	vec3 linearRGB = mix(linearRGBLo, linearRGBHi, vec3(greaterThan(sRGBCol, vec3(0.04045))));
	return linearRGB;
}
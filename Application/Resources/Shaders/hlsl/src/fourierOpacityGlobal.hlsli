#define RESULT_IMAGE_BINDING 0
#define LIGHT_INFO_BINDING 1
#define GLOBAL_MEDIA_BINDING 2
#define LINEAR_SAMPLER_BINDING 3

struct LightInfo
{
	float4x4 invViewProjection;
	float3 position;
	float radius;
	float texelSize;
	uint resolution;
	uint offsetX;
	uint offsetY;
	uint isPointLight;
	float pad1;
	float pad2;
	float pad3;
};

struct PushConsts
{
	uint lightIndex;
	uint globalMediaCount;
};
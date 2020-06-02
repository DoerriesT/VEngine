#define LIGHT_INFO_BINDING 0
#define VOLUME_INFO_BINDING 1

struct LightInfo
{
	float4x4 viewProjection;
	float4x4 invProjection;
	float3 position;
	float depthScale;
	float depthBias;
	float pad0;
	float pad1;
	float pad2;
};

struct VolumeInfo
{
	float4 localToWorld0;
	float4 localToWorld1;
	float4 localToWorld2;
	float4 worldToLocal0;
	float4 worldToLocal1;
	float4 worldToLocal2;
	float extinction;
	float pad0;
	float pad1;
	float pad2;
};

struct PushConsts
{
	uint lightIndex;
	uint volumeIndex;
};
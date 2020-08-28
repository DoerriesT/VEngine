#define DEPTH_IMAGE_BINDING 0
#define RESULT_IMAGE_BINDING 1
#define NORMAL_IMAGE_BINDING 2
#define CONSTANT_BUFFER_BINDING 3

struct Constants
{
	float4x4 viewMatrix;
	float4 unprojectParams;
	float2 texelSize;
	uint width;
	uint height;
	float radiusScale;
	float falloffScale;
	float falloffBias;
	uint sampleCount;
	uint frame;
	float maxTexelRadius;
};

struct PushConsts
{
	float4 unprojectParams;
	float2 texelSize;
	uint width;
	uint height;
	float radiusScale;
	float falloffScale;
	float falloffBias;
	uint sampleCount;
	uint frame;
};
#define VOLUME_TRANSFORMS_BINDING 0
#define MATRIX_BUFFER_BINDING 1
#define LOCAL_MEDIA_BINDING 2
#define DEPTH_RANGE_IMAGE_BINDING 3
#define LINEAR_SAMPLER_BINDING 4


struct PushConsts
{
	float3 rayDir;
	float invLightRange;
	float3 topLeft;
	uint volumeIndex;
	float3 deltaX;
	uint shadowMatrixIndex;
	float3 deltaY;
};
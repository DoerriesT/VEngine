#define RESULT_IMAGE_BINDING 0
#define EXTINCTION_IMAGE_BINDING 1

struct PushConsts
{
	float4x4 invViewProjection;
	float3 coordBias;
	float coordScale;
};
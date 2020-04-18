#define INPUT_IMAGE_SET 0
#define INPUT_IMAGE_BINDING 0
#define RESULT_IMAGE_SET 0
#define RESULT_IMAGE_BINDING 1
#define VELOCITY_IMAGE_SET 0
#define VELOCITY_IMAGE_BINDING 2
#define HISTORY_IMAGE_SET 0
#define HISTORY_IMAGE_BINDING 3
#define LINEAR_SAMPLER_SET 0
#define LINEAR_SAMPLER_BINDING 4
#define CONSTANT_BUFFER_SET 0
#define CONSTANT_BUFFER_BINDING 5

struct Constants
{
	float4x4 invViewProjection;
	float4x4 prevInvViewProjection;
	float2 texelSize;
	uint width;
	uint height;
	float nearPlane;
	float farPlane;
	uint ignoreHistory;
};
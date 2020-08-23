#define INPUT_IMAGE_BINDING 0
#define RESULT_IMAGE_BINDING 1
#define VELOCITY_IMAGE_BINDING 2
#define HISTORY_IMAGE_BINDING 3
#define CONSTANT_BUFFER_BINDING 4

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
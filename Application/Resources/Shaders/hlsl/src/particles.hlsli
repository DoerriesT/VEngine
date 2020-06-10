#define PARTICLES_BINDING 0
#define CONSTANT_BUFFER_BINDING 1

struct Constants
{
	float4x4 viewProjectionMatrix;
	float3 cameraPosition;
	float pad0;
	float3 cameraUp;
	float pad1;
};
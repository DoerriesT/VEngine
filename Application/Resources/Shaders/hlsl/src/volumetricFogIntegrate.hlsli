#define RESULT_IMAGE_BINDING 0
#define INPUT_IMAGE_BINDING 1
#define CONSTANT_BUFFER_BINDING 2

struct Constants
{
	float3 frustumCornerTL;
	uint volumeDepth;
	float3 frustumCornerTR;
	float volumeNear;
	float3 frustumCornerBL;
	float volumeFar;
	float3 frustumCornerBR;
	float pad0;
	float3 cameraPos;
};
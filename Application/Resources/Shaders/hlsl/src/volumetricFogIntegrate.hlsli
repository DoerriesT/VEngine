#define RESULT_IMAGE_BINDING 0
#define INPUT_IMAGE_BINDING 1
#define CONSTANT_BUFFER_BINDING 2

struct Constants
{
	float3 frustumCornerTL;
	float pad0;
	float3 frustumCornerTR;
	float pad1;
	float3 frustumCornerBL;
	float pad3;
	float3 frustumCornerBR;
	float pad4;
	float3 cameraPos;
};
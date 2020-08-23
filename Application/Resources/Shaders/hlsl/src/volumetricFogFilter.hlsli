#define RESULT_IMAGE_BINDING 0
#define INPUT_IMAGE_BINDING 1
#define HISTORY_IMAGE_BINDING 2
#define CONSTANT_BUFFER_BINDING 3
#define EXPOSURE_DATA_BUFFER_BINDING 4

struct PushConsts
{
	float alpha;
	uint neighborHoodClamping;
	uint advancedFilter;
};

struct Constants
{
	float4x4 prevViewMatrix;
	float4x4 prevProjMatrix;
	float4 reprojectedTexCoordScaleBias;
	float3 frustumCornerTL;
	uint ignoreHistory;
	float3 frustumCornerTR;
	float pad1;
	float3 frustumCornerBL;
	float pad2;
	float3 frustumCornerBR;
	uint pad3;
	float3 cameraPos;
	
};
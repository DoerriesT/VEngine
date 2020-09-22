#define RESULT_IMAGE_BINDING 0
#define INPUT_IMAGE_BINDING 1
#define HISTORY_IMAGE_BINDING 2
#define CONSTANT_BUFFER_BINDING 3
#define EXPOSURE_DATA_BUFFER_BINDING 4
#define TILE_DEPTH_IMAGE_BINDING 5
#define PREV_TILE_DEPTH_IMAGE_BINDING 6

struct Constants
{
	float4x4 prevViewMatrix;
	float4x4 prevProjMatrix;
	float3 frustumCornerTL;
	uint ignoreHistory;
	float3 frustumCornerTR;
	uint checkerBoardCondition;
	float3 frustumCornerBL;
	float alpha;
	float3 frustumCornerBR;
	uint pad0;
	float3 cameraPos;
	uint volumeDepth;
	float volumeNear;
	float volumeFar;
};
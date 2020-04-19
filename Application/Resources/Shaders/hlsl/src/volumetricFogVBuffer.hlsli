#define SCATTERING_EXTINCTION_IMAGE_SET 0
#define SCATTERING_EXTINCTION_IMAGE_BINDING 0
#define EMISSIVE_PHASE_IMAGE_SET 0
#define EMISSIVE_PHASE_IMAGE_BINDING 1
#define GLOBAL_MEDIA_SET 0
#define GLOBAL_MEDIA_BINDING 2
#define LOCAL_MEDIA_SET 0
#define LOCAL_MEDIA_BINDING 3
#define LOCAL_MEDIA_Z_BINS_SET 0
#define LOCAL_MEDIA_Z_BINS_BINDING 4
#define LOCAL_MEDIA_BIT_MASK_SET 0
#define LOCAL_MEDIA_BIT_MASK_BINDING 5
#define CONSTANT_BUFFER_SET 0
#define CONSTANT_BUFFER_BINDING 6

struct Constants
{
	float4x4 viewMatrix;
	float3 frustumCornerTL;
	float jitterX;
	float3 frustumCornerTR;
	float jitterY;
	float3 frustumCornerBL;
	float jitterZ;
	float3 frustumCornerBR;
	uint globalMediaCount;
	float3 cameraPos;
	uint localMediaCount;
};
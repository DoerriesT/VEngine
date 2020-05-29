#define EXTINCTION_IMAGE_BINDING 0
#define GLOBAL_MEDIA_BINDING 1
#define LOCAL_MEDIA_BINDING 2

struct PushConsts
{
	float3 positionBias;
	float positionScale;
	uint globalMediaCount;
	uint localMediaCount;
	uint dstOffset;
};
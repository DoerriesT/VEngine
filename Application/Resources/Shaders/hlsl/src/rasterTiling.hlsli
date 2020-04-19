#define PUNCTUAL_LIGHTS_BIT_MASK_SET 0
#define PUNCTUAL_LIGHTS_BIT_MASK_BINDING 0
#define PUNCTUAL_LIGHTS_SHADOWED_BIT_MASK_SET 0
#define PUNCTUAL_LIGHTS_SHADOWED_BIT_MASK_BINDING 1
#define PARTICIPATING_MEDIA_BIT_MASK_SET 0
#define PARTICIPATING_MEDIA_BIT_MASK_BINDING 2

struct PushConsts
{
	float4x4 transform;
	uint index;
	uint alignedDomainSizeX;
	uint wordCount;
	uint targetBuffer;
};
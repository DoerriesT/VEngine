#define IRRADIANCE_VOLUME_IMAGE_SET 0
#define IRRADIANCE_VOLUME_IMAGE_BINDING 0

#define IRRADIANCE_VOLUME_WIDTH_CONST_ID 0
#define IRRADIANCE_VOLUME_HEIGHT_CONST_ID 1
#define IRRADIANCE_VOLUME_DEPTH_CONST_ID 2
#define IRRADIANCE_VOLUME_CASCADES_CONST_ID 3

struct PushConsts
{
	mat4 jitteredViewProjectionMatrix;
	vec4 cameraPosition;
	float scale;
	uint cascadeIndex;
};
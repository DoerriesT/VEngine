#define AGE_IMAGE_SET 0
#define AGE_IMAGE_BINDING 0
#define QUEUE_BUFFER_SET 0
#define QUEUE_BUFFER_BINDING 1
#define IRRADIANCE_VOLUME_IMAGE_SET 0
#define IRRADIANCE_VOLUME_IMAGE_BINDING 2
#define RAY_IMAGE_SET 0
#define RAY_IMAGE_BINDING 3

#define GRID_WIDTH_CONST_ID 0
#define GRID_HEIGHT_CONST_ID 1
#define GRID_DEPTH_CONST_ID 2
#define GRID_BASE_SCALE_CONST_ID 3
#define CASCADES_CONST_ID 4
#define PROBE_SIDE_LENGTH_CONST_ID 5

struct PushConsts
{
	vec4 cameraPosition;
	float time;
};
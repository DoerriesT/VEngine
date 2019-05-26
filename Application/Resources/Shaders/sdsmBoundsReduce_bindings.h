#define DEPTH_IMAGE_SET 0
#define DEPTH_IMAGE_BINDING 0
#define PARTITION_BOUNDS_SET 0
#define PARTITION_BOUNDS_BINDING 1
#define SPLITS_SET 0
#define SPLITS_BINDING 2

struct PushConsts
{
	mat4 invProjection;
	mat4 cameraViewToLightView;
};
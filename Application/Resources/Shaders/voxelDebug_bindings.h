#define VOXEL_IMAGE_SET 0
#define VOXEL_IMAGE_BINDING 0
#define VOXEL_POSITIONS_SET 0
#define VOXEL_POSITIONS_BINDING 1

struct PushConsts
{
	mat4 jitteredViewProjectionMatrix;
	vec4 cameraPosition;
	float scale;
};
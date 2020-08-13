#define PARTICLES_BINDING 0
#define MATRIX_BUFFER_BINDING 1
#define LIGHT_DIR_BUFFER_BINDING 2


struct PushConsts
{
	uint shadowMatrixIndex;
	uint directionIndex;
	uint particleOffset;
};
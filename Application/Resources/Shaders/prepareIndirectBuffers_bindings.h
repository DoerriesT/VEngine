#include "common.h"

#define INSTANCE_DATA_SET 0
#define INSTANCE_DATA_BINDING 0
#define SUB_MESH_DATA_SET 0
#define SUB_MESH_DATA_BINDING 1
#define OPAQUE_INDIRECT_BUFFER_SET 0
#define OPAQUE_INDIRECT_BUFFER_BINDING 2
#define MASKED_INDIRECT_BUFFER_SET 0
#define MASKED_INDIRECT_BUFFER_BINDING 3
#define TRANSPARENT_INDIRECT_BUFFER_SET 0
#define TRANSPARENT_INDIRECT_BUFFER_BINDING 4
#define OPAQUE_SHADOW_INDIRECT_BUFFER_SET 0
#define OPAQUE_SHADOW_INDIRECT_BUFFER_BINDING 5
#define MASKED_SHADOW_INDIRECT_BUFFER_SET 0
#define MASKED_SHADOW_INDIRECT_BUFFER_BINDING 6


struct SubMeshData
{
	uint indexCount;
	uint firstIndex;
	int vertexOffset;
};

struct DrawIndexedIndirectCommand 
{
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int vertexOffset;
    uint firstInstance;
};

struct PushConsts
{
	uint opaqueCount;
	uint maskedCount;
	uint transparentCount;
	uint opaqueShadowCount;
	uint maskedShadowCount;
};
#version 450

#include "shadows_bindings.h"

#ifndef ALPHA_MASK_ENABLED
#define ALPHA_MASK_ENABLED 0
#endif // ALPHA_MASK_ENABLED

layout(set = INSTANCE_DATA_SET, binding = INSTANCE_DATA_BINDING) readonly buffer INSTANCE_DATA 
{
    SubMeshInstanceData uInstanceData[];
};

layout(set = TRANSFORM_DATA_SET, binding = TRANSFORM_DATA_BINDING) readonly buffer TRANSFORM_DATA 
{
    mat4 uTransformData[];
};

layout(set = MATERIAL_DATA_SET, binding = MATERIAL_DATA_BINDING) readonly buffer MATERIAL_DATA 
{
    MaterialData uMaterialData[];
};

layout(push_constant) uniform PUSH_CONSTS 
{
	PushConsts uPushConsts;
};

layout(location = 0) in vec3 inPosition;
#if ALPHA_MASK_ENABLED
layout(location = 1) in vec2 inTexCoord;


layout(location = 0) out vec2 vTexCoord;
layout(location = 1) flat out uint vAlbedoTextureIndex;
#endif //ALPHA_MASK_ENABLED

void main() 
{
	SubMeshInstanceData instanceData = uInstanceData[gl_InstanceIndex];
    gl_Position = uPushConsts.shadowMatrix * uTransformData[instanceData.transformIndex] * vec4(inPosition, 1.0);
	
	#if ALPHA_MASK_ENABLED
	vTexCoord = inTexCoord;
	vAlbedoTextureIndex = (uMaterialData[instanceData.materialIndex].albedoNormalTexture & 0xFFFF0000) >> 16;
	#endif //ALPHA_MASK_ENABLED
}


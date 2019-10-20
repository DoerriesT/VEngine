#version 450

#include "voxelDebug_bindings.h"

layout(set = VOXEL_IMAGE_SET, binding = VOXEL_IMAGE_BINDING) uniform sampler3D uVoxelImage;

layout(push_constant) uniform PUSH_CONSTS 
{
	PushConsts uPushConsts;
};

layout(location = 0) flat out vec4 vColor;

void main() 
{
	const ivec3 gridSize = ivec3(textureSize(uVoxelImage, 0).xyz);
	ivec3 cameraCoord = ivec3(round(uPushConsts.cameraPosition.xyz * (1.0 / uPushConsts.scale)));
	ivec3 gridOffset = cameraCoord - gridSize / 2;
	ivec3 globalCoord;
	globalCoord.y = gl_InstanceIndex / (gridSize.x * gridSize.z);
	globalCoord.x = gl_InstanceIndex % gridSize.x;
	globalCoord.z = (gl_InstanceIndex % (gridSize.x * gridSize.z)) / gridSize.z;
	globalCoord += gridOffset;
	
	ivec3 globalCoordWrapped = ivec3(fract(globalCoord / vec3(gridSize)) * gridSize);
	
	vColor = texelFetch(uVoxelImage, globalCoordWrapped, 0).rgba;
	
	vec3 position = vec3(((gl_VertexIndex & 0x4) == 0) ? -0.5: 0.5,
							((gl_VertexIndex & 0x2) == 0) ? -0.5 : 0.5,
							((gl_VertexIndex & 0x1) == 0) ? -0.5 : 0.5);
					
	position += vec3(globalCoord);					
	position *= uPushConsts.scale;
	
    gl_Position = uPushConsts.jitteredViewProjectionMatrix * vec4(position, 1.0);
	
	if (vColor.a == 0.0)
	{
		gl_Position = vec4(0.0);
	}
	
	
}


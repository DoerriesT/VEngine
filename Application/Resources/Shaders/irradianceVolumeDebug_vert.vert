#version 450

#include "irradianceVolumeDebug_bindings.h"

layout(constant_id = IRRADIANCE_VOLUME_WIDTH_CONST_ID) const uint cVolumeWidth = 64;
layout(constant_id = IRRADIANCE_VOLUME_HEIGHT_CONST_ID) const uint cVolumeHeight = 32;
layout(constant_id = IRRADIANCE_VOLUME_DEPTH_CONST_ID) const uint cVolumeDepth = 64;

layout(push_constant) uniform PUSH_CONSTS 
{
	PushConsts uPushConsts;
};

layout(location = 0) flat out vec3 vCoord;
layout(location = 1) out vec3 vNormal;

void main() 
{
	const ivec3 gridSize = ivec3(cVolumeWidth, cVolumeHeight, cVolumeDepth);
	ivec3 cameraCoord = ivec3(round(uPushConsts.cameraPosition.xyz * (1.0 / uPushConsts.scale)));
	ivec3 gridOffset = cameraCoord - gridSize / 2;
	ivec3 globalCoord;
	globalCoord.y = gl_InstanceIndex / (gridSize.x * gridSize.z);
	globalCoord.x = gl_InstanceIndex % gridSize.x;
	globalCoord.z = (gl_InstanceIndex % (gridSize.x * gridSize.z)) / gridSize.z;
	globalCoord += gridOffset;
	
	vCoord = fract(globalCoord / vec3(gridSize));
	
	vec3 position = vec3(((gl_VertexIndex & 0x4) == 0) ? -0.5: 0.5,
							((gl_VertexIndex & 0x2) == 0) ? -0.5 : 0.5,
							((gl_VertexIndex & 0x1) == 0) ? -0.5 : 0.5);
	
	vNormal = position;
	position *= 0.1;
					
	position += vec3(globalCoord);					
	position *= uPushConsts.scale;
		
	gl_Position = uPushConsts.jitteredViewProjectionMatrix * vec4(position, 1.0);
}


#version 450

#include "voxelization_bindings.h"

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;


layout(location = 0) in vec2 vTexCoord[];
layout(location = 1) in vec3 vNormal[];
layout(location = 2) flat in uint vMaterialIndex[];

layout(location = 0) out vec2 vTexCoordG;
layout(location = 1) out vec3 vNormalG;
layout(location = 2) out vec3 vWorldPosG;
layout(location = 3) flat out uint vMaterialIndexG;

layout(push_constant) uniform PUSH_CONSTS 
{
	PushConsts uPushConsts;
};

void main()
{
	vec3 faceNormal = abs(vNormal[0] + vNormal[1] + vNormal[2]);
	uint maxIdx = faceNormal[1] > faceNormal[0] ? 1 : 0;
	maxIdx = faceNormal[2] > faceNormal[maxIdx] ? 2 : maxIdx;
	
	for(uint i = 0; i < 3; ++i)
	{
		const vec3 worldPos = gl_in[i].gl_Position.xyz;
		// world space to voxel grid space
		gl_Position.xyz = worldPos * uPushConsts.voxelScale - uPushConsts.gridOffset.xyz;
		gl_Position.xyz *= uPushConsts.superSamplingFactor;
		
		// project onto dominant axis
		if (maxIdx == 0)
		{
			gl_Position.xyz = gl_Position.zyx;
		}
		else if (maxIdx == 1)
		{
			gl_Position.xyz = gl_Position.xzy;
		}
		
		// voxel grid space to clip space
		gl_Position.xyz *= uPushConsts.invGridResolution;
		gl_Position.xy = gl_Position.xy * 2.0 - 1.0;
		gl_Position.w = 1.0;
		//gl_Position.zw = vec2(1.0);
		
		vTexCoordG = vTexCoord[i];
		vNormalG = vNormal[i];
		vWorldPosG = worldPos;
		vMaterialIndexG = vMaterialIndex[i];
		
		EmitVertex();
    }
 
    EndPrimitive();
}
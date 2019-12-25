#version 450

#include "voxelizationMark_bindings.h"

layout(constant_id = VOXEL_SCALE_CONST_ID) const float cVoxelScale = 1.0;

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

layout(location = 0) out vec3 vWorldPosG;

layout(push_constant) uniform PUSH_CONSTS 
{
	PushConsts uPushConsts;
};

void main()
{
	vec3 faceNormal = abs(normalize(cross(gl_in[1].gl_Position.xyz - gl_in[0].gl_Position.xyz, gl_in[2].gl_Position.xyz - gl_in[0].gl_Position.xyz)));
	uint maxIdx = faceNormal[1] > faceNormal[0] ? 1 : 0;
	maxIdx = faceNormal[2] > faceNormal[maxIdx] ? 2 : maxIdx;
	
	for(uint i = 0; i < 3; ++i)
	{
		const vec3 worldPos = gl_in[i].gl_Position.xyz;
		// world space to voxel grid space
		gl_Position.xyz = worldPos * cVoxelScale - uPushConsts.gridOffset.xyz;
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
		
		vWorldPosG = worldPos;
		
		EmitVertex();
    }
 
    EndPrimitive();
}